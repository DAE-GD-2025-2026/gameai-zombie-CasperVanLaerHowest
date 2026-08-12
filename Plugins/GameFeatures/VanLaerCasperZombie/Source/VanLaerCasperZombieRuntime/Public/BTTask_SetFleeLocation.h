#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SetFleeLocation.generated.h"

UCLASS()
class VANLAERCASPERZOMBIERUNTIME_API UBTTask_SetFleeLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SetFleeLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName MoveLocationKey{TEXT("MoveLocation")};

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName TargetZombieKey{TEXT("TargetZombie")};

	UPROPERTY(EditAnywhere, Category = "Flee")
	float FleeDistance{1200.0f};

	UPROPERTY(EditAnywhere, Category = "Flee")
	float SearchRadius{600.0f};

};
