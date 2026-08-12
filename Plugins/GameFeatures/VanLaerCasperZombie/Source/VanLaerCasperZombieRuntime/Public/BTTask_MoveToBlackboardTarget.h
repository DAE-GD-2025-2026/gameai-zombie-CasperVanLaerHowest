#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MoveToBlackboardTarget.generated.h"

UCLASS()
class VANLAERCASPERZOMBIERUNTIME_API UBTTask_MoveToBlackboardTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MoveToBlackboardTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	bool GetTargetLocation(UBlackboardComponent& Blackboard, FVector& OutLocation) const;
	class AActor* GetTargetActor(UBlackboardComponent& Blackboard) const;
	bool FindReachablePointInsideHouse(class APawn& Pawn, class AHouse& House, FVector& OutLocation) const;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName TargetKey{TEXT("MoveLocation")};

	UPROPERTY(EditAnywhere, Category = "Movement")
	float AcceptableRadius{120.0f};

};
