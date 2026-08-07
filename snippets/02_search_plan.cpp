// Awake in Hell — building a search plan (excerpt)
//
// When the pursuer loses sight of the player it does not wander. It walks the
// last known position first, then the nearest rooms, then rolls a die on each
// nearby hiding place.
//
// The important detail is that "nearest" means NavMesh distance, not straight
// line. A room on the other side of a wall is two metres away and thirty
// seconds of walking; treating those as the same number makes an AI that
// searches in a way players read as stupid.

void AAIHPursuerController::BuildSearchPlan()
{
    SearchQueue.Reset();
    SearchSpots.Reset();
    SearchIndex = 0;
    SearchRadius = 0.f;

    if (!bHasLastKnown)
    {
        return;
    }

    SearchQueue.Add(LastKnownPosition);   // always look where they actually were
    SearchSpots.Add(nullptr);

    struct FCandidate { FVector Location; float Distance; };
    TArray<FCandidate> Candidates;

    for (TActorIterator<AAIHPatrolPoint> It(GetWorld()); It; ++It)
    {
        if (!It->bRoomVisit)
        {
            continue;
        }
        float Distance = 0.f;
        if (!NavDistance(LastKnownPosition, It->GetActorLocation(), Distance))
        {
            continue;
        }
        if (Distance > 3000.f)
        {
            continue;
        }
        Candidates.Add({It->GetActorLocation(), Distance});
    }

    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Distance < B.Distance; });

    for (int32 i = 0; i < FMath::Min(SearchRoomCount, Candidates.Num()); ++i)
    {
        SearchQueue.Add(Candidates[i].Location);
        SearchSpots.Add(nullptr);
        SearchRadius = FMath::Max(SearchRadius, Candidates[i].Distance);
    }

    // Hiding places near the last known position, each on a weighted coin flip.
    // Deliberately below 1: the player must not be able to know whether the
    // door will be opened. That uncertainty is the mechanic — a guaranteed
    // check makes hiding useless, a guaranteed miss makes it a free escape.
    for (TActorIterator<AAIHHidingSpot> It(GetWorld()); It; ++It)
    {
        float Distance = 0.f;
        if (!NavDistance(LastKnownPosition, It->GetActorLocation(), Distance) || Distance > 2200.f)
        {
            continue;
        }
        if (FMath::FRand() > HideCheckChance)   // 0.4
        {
            continue;
        }
        SearchQueue.Add(It->GetActorLocation());
        SearchSpots.Add(*It);
        SearchRadius = FMath::Max(SearchRadius, Distance);
    }
}

// Straight-line distance would say the room behind you is close. It is not.
bool AAIHPursuerController::NavDistance(const FVector& From, const FVector& To, float& OutDistance) const
{
    const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav)
    {
        return false;
    }
    double Length = 0.0;
    if (UNavigationSystemV1::GetPathLength(GetWorld(), From, To, Length) != ENavigationQueryResult::Success)
    {
        return false;
    }
    OutDistance = static_cast<float>(Length);
    return true;
}
