#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_AttackTargetZombie.generated.h"

UCLASS()
class VANLAERCASPERZOMBIERUNTIME_API UBTTask_AttackTargetZombie : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_AttackTargetZombie();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	int32 FindLoadedWeaponSlot(const class UInventoryComponent& Inventory) const;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName TargetZombieKey{TEXT("TargetZombie")};

	UPROPERTY(EditAnywhere, Category = "Attack", meta = (ClampMin = "0.1"))
	float FireInterval{0.75f};

	UPROPERTY(EditAnywhere, Category = "Attack", meta = (ClampMin = "100.0"))
	float MaximumAttackRange{1200.0f};

	float TimeUntilNextShot{0.0f};
};
