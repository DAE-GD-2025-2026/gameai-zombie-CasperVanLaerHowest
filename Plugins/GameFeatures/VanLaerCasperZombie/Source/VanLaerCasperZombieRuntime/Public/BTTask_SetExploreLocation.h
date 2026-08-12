#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SetExploreLocation.generated.h"

UCLASS()
class VANLAERCASPERZOMBIERUNTIME_API UBTTask_SetExploreLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SetExploreLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName MoveLocationKey{TEXT("MoveLocation")};

	UPROPERTY(EditAnywhere, Category = "Explore")
	float MaxOffset{1500.0f};

	UPROPERTY(EditAnywhere, Category = "Explore")
	float ReplanDistance{250.0f};

	UPROPERTY(EditAnywhere, Category = "Explore")
	float MinExploreDistance{700.0f};

};
