// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Damage.h"
#include "StudentPerceptor.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class VANLAERCASPERZOMBIERUNTIME_API UStudentPerceptor : public UActorComponent
{
	GENERATED_BODY()

public:
	UStudentPerceptor();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	virtual void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void OnTrackedZombieDestroyed(AActor* DestroyedActor);

	bool StartHouseScan(class AHouse* House);
	bool IsHouseScanComplete(const AHouse* House) const;
	void ForgetRememberedItem(class ABaseItem* Item);

private:
	struct FRememberedHouse
	{
		TWeakObjectPtr<AHouse> Actor{};
		FVector Location{FVector::ZeroVector};
		bool bExplored{false};
	};

	struct FRememberedItem
	{
		TWeakObjectPtr<ABaseItem> Actor{};
		FVector Location{FVector::ZeroVector};
	};

	enum class EScanPhase : uint8
	{
		StartupWait,
		Rotating,
		None
	};

	class AAIController* GetAIController() const;
	class APawn* GetSurvivorPawn() const;
	class UBlackboardComponent* GetBlackboard() const;
	void TryBindPerception();
	void EnsureFSMRunning() const;
	void UpdateSurvivalBlackboard();
	void TickScan(float DeltaTime);
	void RememberHouse(AHouse& House);
	void RememberItem(ABaseItem& Item);
	void UpdateRememberedTargets(UBlackboardComponent& Blackboard);
	void MaintainInventory();
	void RefreshThreatTarget(UBlackboardComponent& Blackboard);
	void CancelActiveScan();
	void UpdateRunningMode(const UBlackboardComponent& Blackboard) const;
	AHouse* FindClosestHouse(const FVector& From, bool bOnlyUnexplored) const;
	ABaseItem* FindBestNeededItem(const FVector& From) const;

	bool bPerceptionBound{false};
	EScanPhase ScanPhase{EScanPhase::StartupWait};
	float StartupWaitRemaining{1.0f};
	float AccumulatedScanDegrees{0.0f};
	float ScanDegreesPerSecond{120.0f};
	bool bStartupScanCompleted{false};
	bool bResumeStartupScan{false};
	TWeakObjectPtr<AHouse> ScanningHouse{};
	TSet<TWeakObjectPtr<class ABaseZombie>> PerceivedZombies{};
	TArray<FRememberedHouse> RememberedHouses{};
	TArray<FRememberedItem> RememberedItems{};

};
