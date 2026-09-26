UENUM()
enum class EAIHPursuerState : uint8
{
    Patrol,
    Investigate,
    Chase,
    Search
};

void AAIHPursuerController::EnterState(EAIHPursuerState NewState)
{
    State = NewState;
    StateTime = 0.f;
    LookTimer = 0.f;
    bMoveGoalValid = false;
    StopMovement();

    switch (State)
    {
    case EAIHPursuerState::Patrol:
        Pursuer->SetMoveSpeed(Pursuer->PatrolSpeed);
        StartRoute(NAME_None);
        break;

    case EAIHPursuerState::Chase:

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

    if (!Stimulus.WasSuccessfullySensed() || State == EAIHPursuerState::Chase)
    {
        return;
    }

    LastKnownPosition = Stimulus.StimulusLocation;
    bHasLastKnown = true;

    if (State == EAIHPursuerState::Search)
    {

        SearchDeadline += SearchNoiseBonus;
        BuildSearchPlan();
        return;
    }

    InvestigateTarget = Stimulus.StimulusLocation;
    EnterState(EAIHPursuerState::Investigate);
}

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
