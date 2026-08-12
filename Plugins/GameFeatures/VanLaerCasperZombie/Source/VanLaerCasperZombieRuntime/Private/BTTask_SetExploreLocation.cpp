#include "BTTask_SetExploreLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"

namespace
{
	bool IsUsableLocation(const FVector& Location)
	{
		return !Location.ContainsNaN()
			&& FMath::Abs(Location.X) < 1000000.0f
			&& FMath::Abs(Location.Y) < 1000000.0f
			&& FMath::Abs(Location.Z) < 1000000.0f;
	}
}

UBTTask_SetExploreLocation::UBTTask_SetExploreLocation()
{
	NodeName = TEXT("Set Explore Location");
}

EBTNodeResult::Type UBTTask_SetExploreLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!AIController || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	const APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	const FVector ExistingLocation = Blackboard->GetValueAsVector(MoveLocationKey);
	if (IsUsableLocation(ExistingLocation)
		&& FVector::DistSquared2D(Pawn->GetActorLocation(), ExistingLocation) > FMath::Square(ReplanDistance))
	{
		return EBTNodeResult::Succeeded;
	}

	FNavLocation Location{};
	bool bFoundLocation = false;
	for (int Attempts = 0; Attempts < 8; ++Attempts)
	{
		if (NavSystem->GetRandomReachablePointInRadius(Pawn->GetActorLocation(), MaxOffset, Location)
			&& FVector::DistSquared2D(Pawn->GetActorLocation(), Location.Location) >= FMath::Square(MinExploreDistance))
		{
			bFoundLocation = true;
			break;
		}
	}

	if (!bFoundLocation
		&& !NavSystem->GetRandomReachablePointInRadius(Pawn->GetActorLocation(), MaxOffset, Location))
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(MoveLocationKey, Location.Location);
	return EBTNodeResult::Succeeded;
}
