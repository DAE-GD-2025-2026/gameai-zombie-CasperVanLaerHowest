// Fill out your copyright notice in the Description page of Project Settings.


#include "StudentPerceptor.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Common/HealthComponent.h"
#include "Common/StaminaComponent.h"
#include "Common/InventoryComponent.h"
#include "FSMComponent.h"
#include "Zombies/BaseZombie.h"
#include "Items/BaseItem.h"
#include "Items/ItemType.h"
#include "Survivor/SurvivorPawn.h"
#include "Village/House/House.h"

namespace
{
	const FName CurrentStateKey{TEXT("CurrentState")};
	const FName HasThreatKey{TEXT("HasThreat")};
	const FName HasWeaponKey{TEXT("HasWeapon")};
	const FName HasUnexploredHouseKey{TEXT("HasUnexploredHouse")};
	const FName IsScanningKey{TEXT("IsScanning")};
	const FName NeedsFoodKey{TEXT("NeedsFood")};
	const FName NeedsHealthKey{TEXT("NeedsHealth")};
	const FName ShouldHideKey{TEXT("ShouldHide")};
	const FName TargetHouseKey{TEXT("TargetHouse")};
	const FName TargetItemKey{TEXT("TargetItem")};
	const FName TargetZombieKey{TEXT("TargetZombie")};
}

UStudentPerceptor::UStudentPerceptor()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStudentPerceptor::BeginPlay()
{
	Super::BeginPlay();

	TryBindPerception();
	EnsureFSMRunning();
	UpdateSurvivalBlackboard();
}

void UStudentPerceptor::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TryBindPerception();
	TickScan(DeltaTime);
	EnsureFSMRunning();
	UpdateSurvivalBlackboard();
	MaintainInventory();
}

void UStudentPerceptor::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	UBlackboardComponent* Blackboard = GetBlackboard();
	if (!Blackboard)
	{
		return;
	}

	if (Actor->IsA(ABaseZombie::StaticClass()))
	{
		ABaseZombie* Zombie = CastChecked<ABaseZombie>(Actor);
		if (Stimulus.WasSuccessfullySensed())
		{
			Actor->OnDestroyed.AddUniqueDynamic(this, &UStudentPerceptor::OnTrackedZombieDestroyed);
			PerceivedZombies.Add(Zombie);
			CancelActiveScan();
		}
		else
		{
			PerceivedZombies.Remove(Zombie);
		}
		RefreshThreatTarget(*Blackboard);
	}
	else if (Actor->IsA(ABaseItem::StaticClass()))
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			RememberItem(*CastChecked<ABaseItem>(Actor));
		}
	}
	else if (Actor->IsA(AHouse::StaticClass()))
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			RememberHouse(*CastChecked<AHouse>(Actor));
		}
	}
}

void UStudentPerceptor::OnTrackedZombieDestroyed(AActor* DestroyedActor)
{
	UBlackboardComponent* Blackboard = GetBlackboard();
	if (!Blackboard)
	{
		return;
	}

	PerceivedZombies.Remove(Cast<ABaseZombie>(DestroyedActor));
	RefreshThreatTarget(*Blackboard);
}

bool UStudentPerceptor::StartHouseScan(AHouse* House)
{
	if (!House || ScanPhase != EScanPhase::None)
	{
		return false;
	}

	const APawn* Pawn = GetSurvivorPawn();
	const FHouseBounds Bounds = House->GetBounds();
	if (!Pawn
		|| FMath::Abs(Pawn->GetActorLocation().X - Bounds.Origin.X) > Bounds.Extent.X
		|| FMath::Abs(Pawn->GetActorLocation().Y - Bounds.Origin.Y) > Bounds.Extent.Y)
	{
		return false;
	}

	RememberHouse(*House);
	ScanningHouse = House;
	AccumulatedScanDegrees = 0.0f;
	ScanPhase = EScanPhase::Rotating;
	return true;
}

bool UStudentPerceptor::IsHouseScanComplete(const AHouse* House) const
{
	if (!House || ScanPhase != EScanPhase::None)
	{
		return false;
	}

	for (const FRememberedHouse& Memory : RememberedHouses)
	{
		if (Memory.Actor.Get() == House)
		{
			return Memory.bExplored;
		}
	}

	return false;
}

void UStudentPerceptor::ForgetRememberedItem(ABaseItem* Item)
{
	RememberedItems.RemoveAll([Item](const FRememberedItem& Memory)
	{
		return !Memory.Actor.IsValid() || Memory.Actor.Get() == Item;
	});
}

AAIController* UStudentPerceptor::GetAIController() const
{
	if (AAIController* AIController = Cast<AAIController>(GetOwner()))
	{
		return AIController;
	}

	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		return Cast<AAIController>(OwnerPawn->GetController());
	}

	return nullptr;
}

APawn* UStudentPerceptor::GetSurvivorPawn() const
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Pawn;
	}

	if (const AAIController* AIController = GetAIController())
	{
		return AIController->GetPawn();
	}

	return nullptr;
}

UBlackboardComponent* UStudentPerceptor::GetBlackboard() const
{
	if (AAIController* AIController = GetAIController())
	{
		return AIController->GetBlackboardComponent();
	}

	return nullptr;
}

void UStudentPerceptor::TryBindPerception()
{
	if (bPerceptionBound)
	{
		return;
	}

	UAIPerceptionComponent* PerceptionComp = GetOwner()->GetComponentByClass<UAIPerceptionComponent>();
	if (!PerceptionComp)
	{
		if (const AAIController* AIController = GetAIController())
		{
			if (const APawn* Pawn = AIController->GetPawn())
			{
				PerceptionComp = Pawn->GetComponentByClass<UAIPerceptionComponent>();
			}
		}
	}

	if (!PerceptionComp)
	{
		return;
	}

	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &UStudentPerceptor::OnPerceptionUpdated);
	bPerceptionBound = true;
}

void UStudentPerceptor::EnsureFSMRunning() const
{
	AAIController* AIController = GetAIController();
	if (!AIController)
	{
		return;
	}

	UFSMComponent* FSMComponent = AIController->FindComponentByClass<UFSMComponent>();
	if (!FSMComponent)
	{
		FSMComponent = NewObject<UFSMComponent>(AIController, TEXT("SurvivorFSMComponent"));
		AIController->AddInstanceComponent(FSMComponent);
		FSMComponent->RegisterComponent();
	}

	if (!FSMComponent->IsRunning())
	{
		FSMComponent->StartLogic();
	}
}

void UStudentPerceptor::UpdateSurvivalBlackboard()
{
	UBlackboardComponent* Blackboard = GetBlackboard();
	if (!Blackboard)
	{
		return;
	}
	RefreshThreatTarget(*Blackboard);
	if (!Blackboard->GetValueAsBool(HasThreatKey)
		&& bResumeStartupScan
		&& ScanPhase == EScanPhase::None)
	{
		AccumulatedScanDegrees = 0.0f;
		ScanPhase = EScanPhase::Rotating;
		bResumeStartupScan = false;
	}

	const bool bIsScanning = ScanPhase != EScanPhase::None;
	Blackboard->SetValueAsBool(IsScanningKey, bIsScanning);
	if (bIsScanning)
	{
		Blackboard->SetValueAsName(CurrentStateKey, TEXT("Scan"));
	}
	else if (Blackboard->GetValueAsName(CurrentStateKey).IsNone())
	{
		Blackboard->SetValueAsName(CurrentStateKey, TEXT("Explore"));
	}

	const UHealthComponent* HealthComponent = GetOwner()->FindComponentByClass<UHealthComponent>();
	const UStaminaComponent* StaminaComponent = GetOwner()->FindComponentByClass<UStaminaComponent>();

	if (!HealthComponent || !StaminaComponent)
	{
		if (const AAIController* AIController = GetAIController())
		{
			if (const APawn* Pawn = AIController->GetPawn())
			{
				HealthComponent = HealthComponent ? HealthComponent : Pawn->FindComponentByClass<UHealthComponent>();
				StaminaComponent = StaminaComponent ? StaminaComponent : Pawn->FindComponentByClass<UStaminaComponent>();
			}
		}
	}

	const bool bNeedsHealth = HealthComponent
		&& HealthComponent->GetHealth() <= HealthComponent->GetMaxHealth() / 2;
	const bool bNeedsFood = StaminaComponent
		&& StaminaComponent->GetCurrentStamina() <= StaminaComponent->GetMaxStamina() * 0.4f;

	Blackboard->SetValueAsBool(NeedsHealthKey, bNeedsHealth);
	Blackboard->SetValueAsBool(NeedsFoodKey, bNeedsFood);
	Blackboard->SetValueAsBool(ShouldHideKey, Blackboard->GetValueAsBool(HasThreatKey) && bNeedsHealth);

	bool bHasWeapon = false;
	if (const APawn* Pawn = GetSurvivorPawn())
	{
		if (const UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>())
		{
			for (const ABaseItem* Item : Inventory->GetInventory())
			{
				if (Item && Item->GetValue() > 0
					&& (Item->GetItemType() == EItemType::Pistol || Item->GetItemType() == EItemType::Shotgun))
				{
					bHasWeapon = true;
					break;
				}
			}
		}
	}
	Blackboard->SetValueAsBool(HasWeaponKey, bHasWeapon);
	UpdateRememberedTargets(*Blackboard);
	UpdateRunningMode(*Blackboard);
}

void UStudentPerceptor::RefreshThreatTarget(UBlackboardComponent& Blackboard)
{
	const APawn* Pawn = GetSurvivorPawn();
	ABaseZombie* ClosestZombie = nullptr;
	float ClosestDistance = TNumericLimits<float>::Max();
	for (auto It = PerceivedZombies.CreateIterator(); It; ++It)
	{
		ABaseZombie* Zombie = It->Get();
		bool bIsAlive = IsValid(Zombie) && !Zombie->IsActorBeingDestroyed() && !Zombie->IsHidden();
		if (bIsAlive)
		{
			if (const UHealthComponent* Health = Zombie->FindComponentByClass<UHealthComponent>())
			{
				bIsAlive = Health->IsAlive();
			}
		}

		if (!bIsAlive)
		{
			It.RemoveCurrent();
			continue;
		}

		const float Distance = Pawn
			? FVector::DistSquared2D(Pawn->GetActorLocation(), Zombie->GetActorLocation())
			: 0.0f;
		if (!ClosestZombie || Distance < ClosestDistance)
		{
			ClosestZombie = Zombie;
			ClosestDistance = Distance;
		}
	}

	if (ClosestZombie)
	{
		Blackboard.SetValueAsObject(TargetZombieKey, ClosestZombie);
		Blackboard.SetValueAsBool(HasThreatKey, true);
	}
	else
	{
		Blackboard.ClearValue(TargetZombieKey);
		Blackboard.SetValueAsBool(HasThreatKey, false);
	}
}

void UStudentPerceptor::CancelActiveScan()
{
	if (ScanPhase == EScanPhase::None)
	{
		return;
	}

	if (!ScanningHouse.IsValid() && !bStartupScanCompleted)
	{
		bResumeStartupScan = true;
	}

	ScanningHouse.Reset();
	AccumulatedScanDegrees = 0.0f;
	StartupWaitRemaining = 0.0f;
	ScanPhase = EScanPhase::None;
}

void UStudentPerceptor::UpdateRunningMode(const UBlackboardComponent& Blackboard) const
{
	ASurvivorPawn* Survivor = Cast<ASurvivorPawn>(GetSurvivorPawn());
	if (!Survivor)
	{
		return;
	}

	const FName CurrentState = Blackboard.GetValueAsName(CurrentStateKey);
	const bool bShouldRun = CurrentState == TEXT("Flee") || CurrentState == TEXT("Hide");
	if (bShouldRun && !Survivor->IsRunning())
	{
		Survivor->StartRunning();
	}
	else if (!bShouldRun && Survivor->IsRunning())
	{
		Survivor->StopRunning();
	}
}

void UStudentPerceptor::TickScan(float DeltaTime)
{
	APawn* Pawn = GetSurvivorPawn();
	if (!Pawn || ScanPhase == EScanPhase::None)
	{
		return;
	}

	if (ScanPhase == EScanPhase::StartupWait)
	{
		StartupWaitRemaining -= DeltaTime;
		if (StartupWaitRemaining > 0.0f)
		{
			return;
		}

		AccumulatedScanDegrees = 0.0f;
		ScanPhase = EScanPhase::Rotating;
	}

	const float Step = FMath::Min(ScanDegreesPerSecond * DeltaTime, 360.0f - AccumulatedScanDegrees);
	Pawn->AddActorWorldRotation(FRotator(0.0f, Step, 0.0f));
	AccumulatedScanDegrees += Step;
	if (AccumulatedScanDegrees < 360.0f)
	{
		return;
	}

	if (AHouse* House = ScanningHouse.Get())
	{
		for (FRememberedHouse& Memory : RememberedHouses)
		{
			if (Memory.Actor.Get() == House)
			{
				Memory.bExplored = true;
				break;
			}
		}
	}
	else
	{
		bStartupScanCompleted = true;
	}

	ScanningHouse.Reset();
	ScanPhase = EScanPhase::None;
}

void UStudentPerceptor::RememberHouse(AHouse& House)
{
	for (FRememberedHouse& Memory : RememberedHouses)
	{
		if (Memory.Actor.Get() == &House)
		{
			Memory.Location = House.GetBounds().Origin;
			return;
		}
	}

	RememberedHouses.Add({&House, House.GetBounds().Origin, false});
}

void UStudentPerceptor::RememberItem(ABaseItem& Item)
{
	if (Item.GetItemType() == EItemType::Garbage || Item.GetValue() <= 0 || Item.IsHidden())
	{
		return;
	}

	for (FRememberedItem& Memory : RememberedItems)
	{
		if (Memory.Actor.Get() == &Item)
		{
			Memory.Location = Item.GetActorLocation();
			return;
		}
	}

	RememberedItems.Add({&Item, Item.GetActorLocation()});
}

void UStudentPerceptor::UpdateRememberedTargets(UBlackboardComponent& Blackboard)
{
	RememberedHouses.RemoveAll([](const FRememberedHouse& Memory) { return !Memory.Actor.IsValid(); });
	RememberedItems.RemoveAll([](const FRememberedItem& Memory)
	{
		return !Memory.Actor.IsValid() || Memory.Actor->IsHidden() || Memory.Actor->GetValue() <= 0;
	});

	APawn* Pawn = GetSurvivorPawn();
	if (!Pawn)
	{
		return;
	}

	const bool bHasThreat = Blackboard.GetValueAsBool(HasThreatKey);
	AHouse* ClosestHouse = FindClosestHouse(Pawn->GetActorLocation(), !bHasThreat);
	Blackboard.SetValueAsBool(HasUnexploredHouseKey, FindClosestHouse(Pawn->GetActorLocation(), true) != nullptr);
	if (ClosestHouse)
	{
		Blackboard.SetValueAsObject(TargetHouseKey, ClosestHouse);
	}
	else if (!bHasThreat)
	{
		Blackboard.ClearValue(TargetHouseKey);
	}

	if (ScanPhase != EScanPhase::None || bHasThreat)
	{
		return;
	}

	if (ABaseItem* BestItem = FindBestNeededItem(Pawn->GetActorLocation()))
	{
		Blackboard.SetValueAsObject(TargetItemKey, BestItem);
	}
	else
	{
		Blackboard.ClearValue(TargetItemKey);
	}
}

void UStudentPerceptor::MaintainInventory()
{
	APawn* Pawn = GetSurvivorPawn();
	if (!Pawn)
	{
		return;
	}

	UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>();
	const UHealthComponent* Health = Pawn->FindComponentByClass<UHealthComponent>();
	const UStaminaComponent* Stamina = Pawn->FindComponentByClass<UStaminaComponent>();
	if (!Inventory)
	{
		return;
	}

	const bool bUseMedkit = Health && Health->GetHealth() <= Health->GetMaxHealth() / 2;
	const ASurvivorPawn* Survivor = Cast<ASurvivorPawn>(Pawn);
	const bool bUseFood = Stamina
		&& Stamina->GetCurrentStamina() <= KINDA_SMALL_NUMBER
		&& Survivor
		&& Survivor->IsRunning();
	for (int32 Slot = 0; Slot < Inventory->GetInventoryCapacity(); ++Slot)
	{
		ABaseItem* Item = Inventory->GetInventory()[Slot];
		if (!Item)
		{
			continue;
		}

		const bool bShouldUse = (bUseMedkit && Item->GetItemType() == EItemType::Medkit)
			|| (bUseFood && Item->GetItemType() == EItemType::Food);
		if (bShouldUse && Item->GetValue() > 0)
		{
			Inventory->UseItem(Slot);
		}

		if (Item->GetValue() <= 0)
		{
			Inventory->RemoveItem(Slot);
		}
	}
}

AHouse* UStudentPerceptor::FindClosestHouse(const FVector& From, bool bOnlyUnexplored) const
{
	AHouse* Result = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (const FRememberedHouse& Memory : RememberedHouses)
	{
		if (!Memory.Actor.IsValid() || (bOnlyUnexplored && Memory.bExplored))
		{
			continue;
		}
		const float Distance = FVector::DistSquared2D(From, Memory.Location);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Result = Memory.Actor.Get();
		}
	}

	return Result;
}

ABaseItem* UStudentPerceptor::FindBestNeededItem(const FVector& From) const
{
	const APawn* Pawn = GetSurvivorPawn();
	const UInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return nullptr;
	}

	int32 Medkits = 0;
	int32 Food = 0;
	int32 Weapons = 0;
	int32 EmptySlots = 0;
	for (const ABaseItem* Item : Inventory->GetInventory())
	{
		if (!Item)
		{
			++EmptySlots;
		}
		else if (Item->GetItemType() == EItemType::Medkit)
		{
			++Medkits;
		}
		else if (Item->GetItemType() == EItemType::Food)
		{
			++Food;
		}
		else if (Item->GetItemType() == EItemType::Pistol || Item->GetItemType() == EItemType::Shotgun)
		{
			++Weapons;
		}
	}

	if (EmptySlots == 0)
	{
		return nullptr;
	}

	auto FindNearestType = [this, &From](auto Matches)
	{
		ABaseItem* Result = static_cast<ABaseItem*>(nullptr);
		float BestDistance = TNumericLimits<float>::Max();
		for (const FRememberedItem& Memory : RememberedItems)
		{
			ABaseItem* Item = Memory.Actor.Get();
			if (!Item || !Matches(Item->GetItemType()))
			{
				continue;
			}
			const float Distance = FVector::DistSquared2D(From, Memory.Location);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Result = Item;
			}
		}
		return Result;
	};

	if (Medkits == 0)
	{
		if (ABaseItem* Item = FindNearestType([](EItemType Type) { return Type == EItemType::Medkit; }))
		{
			return Item;
		}
	}
	if (Food == 0)
	{
		if (ABaseItem* Item = FindNearestType([](EItemType Type) { return Type == EItemType::Food; }))
		{
			return Item;
		}
	}

	return Weapons < 3 ? FindNearestType([](EItemType Type)
	{
		return Type == EItemType::Pistol || Type == EItemType::Shotgun;
	}) : nullptr;
}
