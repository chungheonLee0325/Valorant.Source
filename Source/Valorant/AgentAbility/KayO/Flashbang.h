﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AgentAbility/FlashProjectile.h"
#include "NiagaraSystem.h"
#include "Flashbang.generated.h"

UCLASS()
class VALORANT_API AFlashbang : public AFlashProjectile
{
	GENERATED_BODY()

public:
	AFlashbang();
	
	UFUNCTION(BlueprintCallable)
	void ActiveProjectileMovement(const bool bAltFire);

private:
	// KAYO 플래시뱅 고유 설정
	const bool bAutoActivate = false;
	const float Speed = 2200;
	const float SpeedAltFire = 830;
	const float Gravity = 0.3f;
	const bool bShouldBounce = true;
	const float Bounciness = 0.8f;
	const float Friction = 0.2f;
	const float EquipTime = 0.8f;
	const float UnequipTime = 0.6f;
	const float AirTimeOnBounce = 0.8f;
	const float MaximumAirTime = 1.6f;
	const float MaximumAirTimeAltFire = 1.0f;
	
	float ExplosionTime;
	float CurrentAirTime = 0.0f;
	bool bElapsedAirTime = false;
	bool bIsAltFire = false;

public:
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh = nullptr;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnProjectileBounced(const FHitResult& ImpactResult, const FVector& ImpactVelocity) override;

	void OnElapsedMaxAirTime();
	
	// FlashProjectile의 ExplodeFlash를 오버라이드
	virtual void ExplodeFlash() override;
};