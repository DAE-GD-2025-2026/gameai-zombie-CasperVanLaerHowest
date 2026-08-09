// Fill out your copyright notice in the Description page of Project Settings.


#include "FSMComponent.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "FSM.h"
#include "GameFramework/Pawn.h"
#include "States/State.h"

namespace
{
	const FName CurrentStateKey{TEXT("CurrentState")};
	const FName HasThreatKey{TEXT("HasThreat")};
	const FName HasWeaponKey{TEXT("HasWeapon")};
	const FName TargetZombieKey{TEXT("TargetZombie")};
	const FName TargetItemKey{TEXT("TargetItem")};
	const FName TargetHouseKey{TEXT("TargetHouse")};
	const FName NeedsHealthKey{TEXT("NeedsHealth")};
	const FName NeedsFoodKey{TEXT("NeedsFood")};
	const FName ShouldHideKey{TEXT("ShouldHide")};
	const FName HasUnexploredHouseKey{TEXT("HasUnexploredHouse")};
	const FName IsScanningKey{TEXT("IsScanning")};

	UBlackboardComponent* GetBlackboard(AAIController& Controller)
	{
		return Controller.GetBlackboardComponent();
	}

	bool HasObject(AAIController& Controller, const FName& Key)
	{
		if (const UBlackboardComponent* Blackboard = GetBlackboard(Controller))
		{
			return Blackboard->GetValueAsObject(Key) != nullptr;
		}

		return false;
	}

	bool GetBool(AAIController& Controller, const FName& Key)
	{
		if (const UBlackboardComponent* Blackboard = GetBlackboard(Controller))
		{
			return Blackboard->GetValueAsBool(Key);
		}

		return false;
	}

	class FBlackboardState final : public GameAI::FSM::State
	{
	public:
		explicit FBlackboardState(const FName& InStateName)
			: StateName(InStateName)
		{
		}

		const char* GetDebugName() const override
		{
			return "BlackboardState";
		}

		void Enter(AAIController& Controller) override
		{
			SetState(Controller);
		}

		void Update(AAIController& Controller, float DeltaTime) override
		{
			SetState(Controller);
		}

	private:
		void SetState(AAIController& Controller) const
		{
			if (UBlackboardComponent* Blackboard = GetBlackboard(Controller))
			{
				Blackboard->SetValueAsName(CurrentStateKey, StateName);
			}
		}

		FName StateName;
	};
}

// Sets default values for this component's properties
UFSMComponent::UFSMComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	FSMInstance = std::make_unique<GameAI::FSM::FSM>();
}

UFSMComponent::~UFSMComponent() = default;


GameAI::FSM::State* UFSMComponent::AddState(std::unique_ptr<GameAI::FSM::State>&& NewState)
{
	if (FSMInstance)
	{
		return FSMInstance->AddState(std::move(NewState));
	}

	return nullptr;
}
void UFSMComponent::AddTransition(GameAI::FSM::State* From, GameAI::FSM::State* To, std::function<bool()> EvalFunc)
{
	if (FSMInstance)
	{
		FSMInstance->AddTransition(From, To, std::move(EvalFunc));
	}
}

// Called when the game starts
void UFSMComponent::BeginPlay()
{
	Super::BeginPlay();
	BuildDefaultSurvivorFSM();
}


// Called every frame
void UFSMComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsRunning || !FSMInstance)
	{
		return;
	}

	if (AAIController* AIController = GetOwningAIController())
	{
		FSMInstance->Update(*AIController, DeltaTime);
	}
}

void UFSMComponent::StartLogic()
{
	Super::StartLogic();

	if (bIsRunning || !FSMInstance)
	{
		return;
	}

	BuildDefaultSurvivorFSM();

	if (AAIController* AIController = GetOwningAIController())
	{
		FSMInstance->Start(*AIController);
		bIsRunning = true;
	}
}

void UFSMComponent::StopLogic(const FString& Reason)
{
	Super::StopLogic(Reason);

	if (!bIsRunning || !FSMInstance)
	{
		return;
	}

	if (AAIController* AIController = GetOwningAIController())
	{
		FSMInstance->Stop(*AIController);
	}

	bIsRunning = false;
}

bool UFSMComponent::IsRunning() const
{
	return bIsRunning;
}

const GameAI::FSM::State* UFSMComponent::GetCurrentState() const
{
	return FSMInstance ? FSMInstance->GetCurrentState() : nullptr;
}

void UFSMComponent::BuildDefaultSurvivorFSM()
{
	if (bDefaultFSMBuilt || !FSMInstance)
	{
		return;
	}

	GameAI::FSM::State* Explore = AddState(std::make_unique<FBlackboardState>(TEXT("Explore")));
	GameAI::FSM::State* SeekItem = AddState(std::make_unique<FBlackboardState>(TEXT("SeekItem")));
	GameAI::FSM::State* Flee = AddState(std::make_unique<FBlackboardState>(TEXT("Flee")));
	GameAI::FSM::State* Hide = AddState(std::make_unique<FBlackboardState>(TEXT("Hide")));
	GameAI::FSM::State* Attack = AddState(std::make_unique<FBlackboardState>(TEXT("Attack")));
	GameAI::FSM::State* SearchHouse = AddState(std::make_unique<FBlackboardState>(TEXT("SearchHouse")));
	GameAI::FSM::State* Scan = AddState(std::make_unique<FBlackboardState>(TEXT("Scan")));

	auto WantsToScan = [this]()
	{
		if (AAIController* AIController = GetOwningAIController())
		{
			return GetBool(*AIController, IsScanningKey);
		}

		return false;
	};

	auto WantsToHide = [this]()
	{
		if (AAIController* AIController = GetOwningAIController())
		{
			return GetBool(*AIController, HasThreatKey)
				&& HasObject(*AIController, TargetHouseKey)
				&& (GetBool(*AIController, ShouldHideKey) || GetBool(*AIController, NeedsHealthKey));
		}

		return false;
	};

	auto WantsToFlee = [this]()
	{
		if (AAIController* AIController = GetOwningAIController())
		{
			const bool bCanHide = HasObject(*AIController, TargetHouseKey)
				&& (GetBool(*AIController, ShouldHideKey) || GetBool(*AIController, NeedsHealthKey));
			return GetBool(*AIController, HasThreatKey) && !bCanHide;
		}

		return false;
	};

	auto WantsItem = [this]()
	{
		if (AAIController* AIController = GetOwningAIController())
		{
			return !GetBool(*AIController, HasThreatKey) && HasObject(*AIController, TargetItemKey);
		}

		return false;
	};

	auto WantsAttack = [this]()
	{
		if (AAIController* AIController = GetOwningAIController())
		{
			return GetBool(*AIController, HasThreatKey)
				&& HasObject(*AIController, TargetZombieKey)
				&& GetBool(*AIController, HasWeaponKey)
				&& !GetBool(*AIController, NeedsHealthKey);
		}

		return false;
	};

	auto WantsExplore = [this]()
	{
		if (AAIController* AIController = GetOwningAIController())
		{
			return !GetBool(*AIController, HasThreatKey)
				&& !HasObject(*AIController, TargetItemKey)
				&& !GetBool(*AIController, HasUnexploredHouseKey);
		}

		return false;
	};

	auto WantsSearchHouse = [this]()
	{
		if (AAIController* AIController = GetOwningAIController())
		{
			return !GetBool(*AIController, HasThreatKey)
				&& !HasObject(*AIController, TargetItemKey)
				&& GetBool(*AIController, HasUnexploredHouseKey)
				&& HasObject(*AIController, TargetHouseKey);
		}

		return false;
	};

	const TArray<GameAI::FSM::State*> States{Explore, SeekItem, Flee, Hide, Attack, SearchHouse, Scan};
	for (GameAI::FSM::State* State : States)
	{
		AddTransition(State, Scan, WantsToScan);
		AddTransition(State, Hide, WantsToHide);
		AddTransition(State, Attack, WantsAttack);
		AddTransition(State, Flee, WantsToFlee);
		AddTransition(State, SeekItem, WantsItem);
		AddTransition(State, SearchHouse, WantsSearchHouse);
		AddTransition(State, Explore, WantsExplore);
	}

	bDefaultFSMBuilt = true;
}

AAIController* UFSMComponent::GetOwningAIController() const
{
	if (AAIController* AIController = Cast<AAIController>(GetOwner()))
	{
		return AIController;
	}

	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Cast<AAIController>(Pawn->GetController());
	}

	return nullptr;
}
