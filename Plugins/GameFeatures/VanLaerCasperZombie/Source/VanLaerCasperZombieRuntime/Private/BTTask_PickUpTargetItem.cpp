#include "BTTask_PickUpTargetItem.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"
#include "StudentPerceptor.h"

UBTTask_PickUpTargetItem::UBTTask_PickUpTargetItem()
{
	NodeName = TEXT("Pick Up Target Item");
}

EBTNodeResult::Type UBTTask_PickUpTargetItem::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!AIController || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AIController->GetPawn();
	ABaseItem* Item = Cast<ABaseItem>(Blackboard->GetValueAsObject(TargetItemKey));
	if (!Pawn || !Item)
	{
		Blackboard->ClearValue(TargetItemKey);
		return EBTNodeResult::Failed;
	}

	UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>();
	if (!Inventory)
	{
		return EBTNodeResult::Failed;
	}

	int32 Medkits = 0;
	int32 Food = 0;
	int32 Weapons = 0;
	for (const ABaseItem* InventoryItem : Inventory->GetInventory())
	{
		if (!InventoryItem)
		{
			continue;
		}

		switch (InventoryItem->GetItemType())
		{
		case EItemType::Medkit:
			++Medkits;
			break;
		case EItemType::Food:
			++Food;
			break;
		case EItemType::Pistol:
		case EItemType::Shotgun:
			++Weapons;
			break;
		default:
			break;
		}
	}

	const EItemType TargetType = Item->GetItemType();
	const bool bInventoryLimitReached = (TargetType == EItemType::Medkit && Medkits >= 1)
		|| (TargetType == EItemType::Food && Food >= 1)
		|| ((TargetType == EItemType::Pistol || TargetType == EItemType::Shotgun) && Weapons >= 3)
		|| TargetType == EItemType::Garbage;
	if (bInventoryLimitReached)
	{
		Blackboard->ClearValue(TargetItemKey);
		return EBTNodeResult::Failed;
	}

	const float PickupRange = Inventory->GetPickupRange();
	if (FVector::DistSquared2D(Pawn->GetActorLocation(), Item->GetActorLocation()) > FMath::Square(PickupRange))
	{
		return EBTNodeResult::Failed;
	}

	for (int SlotIdx = 0; SlotIdx < Inventory->GetInventoryCapacity(); ++SlotIdx)
	{
		if (Inventory->GetInventory()[SlotIdx] != nullptr)
		{
			continue;
		}

		if (Inventory->GrabItem(SlotIdx, Item))
		{
			UStudentPerceptor* Perceptor = AIController->FindComponentByClass<UStudentPerceptor>();
			if (!Perceptor)
			{
				Perceptor = Pawn->FindComponentByClass<UStudentPerceptor>();
			}
			if (Perceptor)
			{
				Perceptor->ForgetRememberedItem(Item);
			}
			Blackboard->ClearValue(TargetItemKey);
			return EBTNodeResult::Succeeded;
		}
	}

	return EBTNodeResult::Failed;
}
