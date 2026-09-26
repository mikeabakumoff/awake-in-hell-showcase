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

    SearchQueue.Add(LastKnownPosition);
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

    for (TActorIterator<AAIHHidingSpot> It(GetWorld()); It; ++It)
    {
        float Distance = 0.f;
        if (!NavDistance(LastKnownPosition, It->GetActorLocation(), Distance) || Distance > 2200.f)
        {
            continue;
        }
        if (FMath::FRand() > HideCheckChance)
        {
            continue;
        }
        SearchQueue.Add(It->GetActorLocation());
        SearchSpots.Add(*It);
        SearchRadius = FMath::Max(SearchRadius, Distance);
    }
}

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
