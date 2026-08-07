// Awake in Hell — pursuer state machine (excerpt)
//
// A hand-written FSM in the AIController rather than StateTree or a Behaviour
// Tree. Both of those need editor-authored assets, and this project has none by
// design. Five states stay readable as an enum with the transitions in one
// place; the migration point past ~15 states is documented in the project notes.

UENUM()
enum class EAIHPursuerState : uint8
{
    Patrol,       // walking a route
    Investigate,  // heard something, going to look
    Chase,        // has the player in sight
    Search        // lost the player, sweeping around the last known position
};

void AAIHPursuerController::EnterState(EAIHPursuerState NewState)
{
    State = NewState;
    StateTime = 0.f;
    LookTimer = 0.f;
    bMoveGoalValid = false;   // drop the old goal, the new state picks its own
    StopMovement();

    switch (State)
    {
    case EAIHPursuerState::Patrol:
        Pursuer->SetMoveSpeed(Pursuer->PatrolSpeed);
        StartRoute(NAME_None);
        break;

    case EAIHPursuerState::Chase:
        // Must beat the player's run speed, or a chase can never end.
        Pursuer->SetMoveSpeed(Pursuer->ChaseSpeed);
        break;

    case EAIHPursuerState::Search:
        Pursuer->SetMoveSpeed(Pursuer->SearchSpeed);
        SearchDeadline = SearchBaseTime;
        BuildSearchPlan();
        break;
    }
}

void AAIHPursuerController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    const bool bSight = Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>();

    // Inside a hiding place the player simply is not seen. Handled here rather
    // than by unregistering the stimuli source, so that hearing still works —
    // climbing in makes a noise, and that noise should still carry.
    const AAIHCharacter* Target = Cast<AAIHCharacter>(Actor);
    if (bSight && Target && Target->IsHiding())
    {
        return;
    }

    if (bSight)
    {
        if (Stimulus.WasSuccessfullySensed())
        {
            SeenTarget = Actor;
            LastKnownPosition = Actor->GetActorLocation();
            bHasLastKnown = true;
            if (State != EAIHPursuerState::Chase)
            {
                EnterState(EAIHPursuerState::Chase);
            }
        }
        else if (State == EAIHPursuerState::Chase)
        {
            LastKnownPosition = Actor->GetActorLocation();
            bHasLastKnown = true;
            SeenTarget = nullptr;
            EnterState(EAIHPursuerState::Search);
        }
        return;
    }

    // Hearing. A noise while already chasing adds nothing.
    if (!Stimulus.WasSuccessfullySensed() || State == EAIHPursuerState::Chase)
    {
        return;
    }

    LastKnownPosition = Stimulus.StimulusLocation;
    bHasLastKnown = true;

    if (State == EAIHPursuerState::Search)
    {
        // Fresh noise mid-search extends it and re-plans, rather than
        // restarting the whole thing and losing what has already been checked.
        SearchDeadline += SearchNoiseBonus;
        BuildSearchPlan();
        return;
    }

    InvestigateTarget = Stimulus.StimulusLocation;
    EnterState(EAIHPursuerState::Investigate);
}

// Issue the move request only when the goal actually changes.
//
// This function used to call MoveToLocation every tick, which aborts and
// re-paths the move sixty times a second and reads on screen as an AI that
// refuses to walk. It also treated a failed path as "arrived", so the pursuer
// silently ticked through its entire patrol route without moving. Making the
// failure loud is what produced the diagnosis.
bool AAIHPursuerController::MoveTowards(const FVector& Target, float Acceptance)
{
    if (!bMoveGoalValid || !MoveGoal.Equals(Target, 50.f))
    {
        MoveGoal = Target;
        bMoveGoalValid = true;

        const EPathFollowingRequestResult::Type Result = MoveToLocation(Target, Acceptance);

        if (Result == EPathFollowingRequestResult::Failed)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("pursuer cannot path to %s — is the navmesh built?"), *Target.ToCompactString());
            bMoveGoalValid = false;
            return true;
        }
        if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
        {
            bMoveGoalValid = false;
            return true;
        }
        return false;
    }

    if (GetMoveStatus() == EPathFollowingStatus::Idle)
    {
        bMoveGoalValid = false;
        return true;
    }
    return false;
}
