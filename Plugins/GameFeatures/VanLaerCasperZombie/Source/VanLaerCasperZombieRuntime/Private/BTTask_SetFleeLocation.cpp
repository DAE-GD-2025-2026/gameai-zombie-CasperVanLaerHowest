#include "BTTask_SetFleeLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"

UBTTask_SetFleeLocation::UBTTask_SetFleeLocation()
{
	NodeName = TEXT("Set Flee Location");
}

EBTNodeResult::Type UBTTask_SetFleeLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!AIController || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	const APawn* Pawn = AIController->GetPawn();
	const AActor* Zombie = Cast<AActor>(Blackboard->GetValueAsObject(TargetZombieKey));
	if (!Pawn || !Zombie)
	{
		return EBTNodeResult::Failed;
	}

	const FVector AwayFromZombie = Pawn->GetActorLocation() - Zombie->GetActorLocation();
	if (AwayFromZombie.IsNearlyZero())
	{
		return EBTNodeResult::Failed;
	}

	const FVector DesiredLocation = Pawn->GetActorLocation() + AwayFromZombie.GetSafeNormal2D() * FleeDistance;

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	FNavLocation Location{};
	if (!NavSystem->GetRandomReachablePointInRadius(DesiredLocation, SearchRadius, Location))
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(MoveLocationKey, Location.Location);
	return EBTNodeResult::Succeeded;
}
