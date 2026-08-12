#include "BTTask_MoveToBlackboardTarget.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Common/InventoryComponent.h"
#include "GameFramework/Pawn.h"
#include "Items/BaseItem.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Village/House/House.h"

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

UBTTask_MoveToBlackboardTarget::UBTTask_MoveToBlackboardTarget()
{
	NodeName = TEXT("Move To Blackboard Target");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_MoveToBlackboardTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!AIController || !AIController->GetPawn() || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	AActor* TargetActor = GetTargetActor(*Blackboard);
	FVector TargetLocation{};
	if (AHouse* House = Cast<AHouse>(TargetActor))
	{
		if (!FindReachablePointInsideHouse(*AIController->GetPawn(), *House, TargetLocation))
		{
			return EBTNodeResult::Failed;
		}
	}
	else if (!GetTargetLocation(*Blackboard, TargetLocation))
	{
		return EBTNodeResult::Failed;
	}

	FAIMoveRequest MoveRequest;
	if (TargetActor && !TargetActor->IsA<AHouse>())
	{
		MoveRequest.SetGoalActor(TargetActor);
	}
	else
	{
		MoveRequest.SetGoalLocation(TargetLocation);
	}

	float RequestAcceptanceRadius = AcceptableRadius;
	if (TargetActor && TargetActor->IsA<ABaseItem>())
	{
		if (const UInventoryComponent* Inventory = AIController->GetPawn()->FindComponentByClass<UInventoryComponent>())
		{
			RequestAcceptanceRadius = FMath::Min(AcceptableRadius, Inventory->GetPickupRange() * 0.75f);
		}
	}

	if (FVector::DistSquared2D(AIController->GetPawn()->GetActorLocation(), TargetLocation)
		<= FMath::Square(RequestAcceptanceRadius))
	{
		return EBTNodeResult::Succeeded;
	}

	MoveRequest.SetAcceptanceRadius(RequestAcceptanceRadius);
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetReachTestIncludesGoalRadius(false);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetProjectGoalLocation(true);
	MoveRequest.SetAllowPartialPath(false);
	MoveRequest.SetCanStrafe(false);

	const FPathFollowingRequestResult MoveResult = AIController->MoveTo(MoveRequest);
	if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		return EBTNodeResult::Succeeded;
	}
	if (MoveResult.Code == EPathFollowingRequestResult::Failed)
	{
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_MoveToBlackboardTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!AIController || !Blackboard)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	const EPathFollowingStatus::Type MoveStatus = AIController->GetMoveStatus();
	if (MoveStatus == EPathFollowingStatus::Moving
		|| MoveStatus == EPathFollowingStatus::Waiting
		|| MoveStatus == EPathFollowingStatus::Paused)
	{
		return;
	}

	if (const UPathFollowingComponent* PathFollowing = AIController->GetPathFollowingComponent();
		PathFollowing && PathFollowing->DidMoveReachGoal())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
}

EBTNodeResult::Type UBTTask_MoveToBlackboardTarget::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		AIController->StopMovement();
	}

	return EBTNodeResult::Aborted;
}

bool UBTTask_MoveToBlackboardTarget::GetTargetLocation(UBlackboardComponent& Blackboard, FVector& OutLocation) const
{
	if (AActor* TargetActor = Cast<AActor>(Blackboard.GetValueAsObject(TargetKey)))
	{
		OutLocation = TargetActor->IsA<AHouse>()
			? CastChecked<AHouse>(TargetActor)->GetBounds().Origin
			: TargetActor->GetActorLocation();
		return IsUsableLocation(OutLocation);
	}

	OutLocation = Blackboard.GetValueAsVector(TargetKey);
	return IsUsableLocation(OutLocation);
}

AActor* UBTTask_MoveToBlackboardTarget::GetTargetActor(UBlackboardComponent& Blackboard) const
{
	return Cast<AActor>(Blackboard.GetValueAsObject(TargetKey));
}

bool UBTTask_MoveToBlackboardTarget::FindReachablePointInsideHouse(
	APawn& Pawn, AHouse& House, FVector& OutLocation) const
{
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn.GetWorld());
	if (!NavSystem)
	{
		return false;
	}

	const FHouseBounds Bounds = House.GetBounds();
	const float InnerX = FMath::Max(Bounds.Extent.X - 50.0f, 50.0f);
	const float InnerY = FMath::Max(Bounds.Extent.Y - 50.0f, 50.0f);
	const TArray<FVector2D> CandidateOffsets
	{
		FVector2D::ZeroVector,
		{ InnerX * 0.4f, 0.0f }, { -InnerX * 0.4f, 0.0f },
		{ 0.0f, InnerY * 0.4f }, { 0.0f, -InnerY * 0.4f },
		{ InnerX * 0.4f, InnerY * 0.4f }, { -InnerX * 0.4f, InnerY * 0.4f },
		{ InnerX * 0.4f, -InnerY * 0.4f }, { -InnerX * 0.4f, -InnerY * 0.4f }
	};

	for (const FVector2D& Offset : CandidateOffsets)
	{
		const FVector Candidate = Bounds.Origin + FVector(Offset.X, Offset.Y, 0.0f);
		FNavLocation Projected{};
		if (!NavSystem->ProjectPointToNavigation(Candidate, Projected, FVector(75.0f, 75.0f, 500.0f)))
		{
			continue;
		}

		if (FMath::Abs(Projected.Location.X - Bounds.Origin.X) > InnerX
			|| FMath::Abs(Projected.Location.Y - Bounds.Origin.Y) > InnerY)
		{
			continue;
		}

		if (FVector::DistSquared2D(Pawn.GetActorLocation(), Projected.Location) <= FMath::Square(AcceptableRadius))
		{
			OutLocation = Projected.Location;
			return true;
		}

		UNavigationPath* Path = NavSystem->FindPathToLocationSynchronously(
			Pawn.GetWorld(), Pawn.GetActorLocation(), Projected.Location, &Pawn);
		if (Path && Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() >= 2)
		{
			OutLocation = Projected.Location;
			return true;
		}
	}

	return false;
}
