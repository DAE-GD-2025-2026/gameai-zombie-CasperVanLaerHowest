#include "BTTask_ScanTargetHouse.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "StudentPerceptor.h"
#include "Village/House/House.h"

namespace
{
	UStudentPerceptor* FindPerceptor(AAIController& Controller)
	{
		if (UStudentPerceptor* Perceptor = Controller.FindComponentByClass<UStudentPerceptor>())
		{
			return Perceptor;
		}

		return Controller.GetPawn()
			? Controller.GetPawn()->FindComponentByClass<UStudentPerceptor>()
			: nullptr;
	}
}

UBTTask_ScanTargetHouse::UBTTask_ScanTargetHouse()
{
	NodeName = TEXT("Scan Target House");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_ScanTargetHouse::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Controller || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	AHouse* House = Cast<AHouse>(Blackboard->GetValueAsObject(TargetHouseKey));
	UStudentPerceptor* Perceptor = FindPerceptor(*Controller);
	if (!House || !Perceptor)
	{
		return EBTNodeResult::Failed;
	}

	if (Perceptor->IsHouseScanComplete(House))
	{
		return EBTNodeResult::Succeeded;
	}

	ActiveHouse = House;
	return Perceptor->StartHouseScan(House) ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

void UBTTask_ScanTargetHouse::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Controller || !Blackboard)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AHouse* House = ActiveHouse.Get();
	UStudentPerceptor* Perceptor = FindPerceptor(*Controller);
	if (!House || !Perceptor)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (Perceptor->IsHouseScanComplete(House))
	{
		ActiveHouse.Reset();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}
