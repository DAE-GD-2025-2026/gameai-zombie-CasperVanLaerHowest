#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ScanTargetHouse.generated.h"

UCLASS()
class VANLAERCASPERZOMBIERUNTIME_API UBTTask_ScanTargetHouse : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ScanTargetHouse();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName TargetHouseKey{TEXT("TargetHouse")};

	TWeakObjectPtr<class AHouse> ActiveHouse{};
};
