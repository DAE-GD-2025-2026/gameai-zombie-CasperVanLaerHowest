#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PickUpTargetItem.generated.h"

UCLASS()
class VANLAERCASPERZOMBIERUNTIME_API UBTTask_PickUpTargetItem : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PickUpTargetItem();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName TargetItemKey{TEXT("TargetItem")};
};
