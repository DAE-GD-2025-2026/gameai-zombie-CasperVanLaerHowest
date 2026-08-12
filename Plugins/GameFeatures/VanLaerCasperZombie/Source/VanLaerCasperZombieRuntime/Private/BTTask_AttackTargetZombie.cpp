#include "BTTask_AttackTargetZombie.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"
#include "Items/ItemType.h"
#include "Navigation/PathFollowingComponent.h"
#include "Zombies/BaseZombie.h"

UBTTask_AttackTargetZombie::UBTTask_AttackTargetZombie()
{
	NodeName = TEXT("Attack Target Zombie");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_AttackTargetZombie::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Controller || !Controller->GetPawn() || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	const ABaseZombie* Target = Cast<ABaseZombie>(Blackboard->GetValueAsObject(TargetZombieKey));
	const UInventoryComponent* Inventory = Controller->GetPawn()->FindComponentByClass<UInventoryComponent>();
	if (!Target || !Inventory || FindLoadedWeaponSlot(*Inventory) == INDEX_NONE)
	{
		return EBTNodeResult::Failed;
	}

	TimeUntilNextShot = 0.0f;
	return EBTNodeResult::InProgress;
}

void UBTTask_AttackTargetZombie::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	ABaseZombie* Target = Blackboard ? Cast<ABaseZombie>(Blackboard->GetValueAsObject(TargetZombieKey)) : nullptr;
	UInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Pawn || !Target || !Inventory || Target->IsActorBeingDestroyed())
	{
		if (Controller)
		{
			Controller->StopMovement();
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - Pawn->GetActorLocation();
	if (ToTarget.SizeSquared2D() > FMath::Square(MaximumAttackRange))
	{
		Controller->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (!Controller->LineOfSightTo(Target))
	{
		if (Controller->GetMoveStatus() != EPathFollowingStatus::Moving)
		{
			Controller->MoveToActor(Target, 200.0f, true, true, true, nullptr, true);
		}
		return;
	}

	if (Controller->GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		Controller->StopMovement();
	}

	const int32 WeaponSlot = FindLoadedWeaponSlot(*Inventory);
	if (WeaponSlot == INDEX_NONE)
	{
		Controller->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (!ToTarget.IsNearlyZero())
	{
		Pawn->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}

	TimeUntilNextShot -= DeltaSeconds;
	if (TimeUntilNextShot > 0.0f)
	{
		return;
	}

	Inventory->UseItem(WeaponSlot);
	ABaseItem* Weapon = Inventory->GetInventory()[WeaponSlot];
	if (Weapon && Weapon->GetValue() <= 0)
	{
		Inventory->RemoveItem(WeaponSlot);
	}
	TimeUntilNextShot = FireInterval;
}

EBTNodeResult::Type UBTTask_AttackTargetZombie::AbortTask(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		Controller->StopMovement();
	}

	return EBTNodeResult::Aborted;
}

int32 UBTTask_AttackTargetZombie::FindLoadedWeaponSlot(const UInventoryComponent& Inventory) const
{
	for (int32 Slot = 0; Slot < Inventory.GetInventoryCapacity(); ++Slot)
	{
		const ABaseItem* Item = Inventory.GetInventory()[Slot];
		if (!Item || Item->GetValue() <= 0)
		{
			continue;
		}

		if (Item->GetItemType() == EItemType::Pistol || Item->GetItemType() == EItemType::Shotgun)
		{
			return Slot;
		}
	}

	return INDEX_NONE;
}
