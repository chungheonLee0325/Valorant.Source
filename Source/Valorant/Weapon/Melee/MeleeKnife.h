// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/BaseWeapon.h"
#include "MeleeKnife.generated.h"

UCLASS()
class VALORANT_API AMeleeKnife : public ABaseWeapon
{
	GENERATED_BODY()

public:
	AMeleeKnife();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UAnimMontage* AM_Fire1_1P;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UAnimMontage* AM_Fire1_3P;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UAnimMontage* AM_Fire2_1P;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UAnimMontage* AM_Fire2_3P;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UAnimMontage* AM_Fire3_1P;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UAnimMontage* AM_Fire3_3P;
	
	UPROPERTY(Replicated)
	bool bIsAttacking = false;
	UPROPERTY(Replicated)
	bool bIsCombo = false;
	UPROPERTY(Replicated)
	bool bIsComboTransition = false;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual bool ServerOnly_CanAutoPickUp(ABaseAgent* Agent) const override;
	virtual bool ServerOnly_CanDrop() const override;

	virtual void ServerRPC_SetActive_Implementation(bool bActive) override;

	virtual void StartFire() override;
	virtual void Fire() override;

	UFUNCTION(Server, Reliable)
	void ServerRPC_DamageBox(FVector center, FRotator rot);
	void DamageBox(FVector center, FRotator rot);

	void ResetCombo();

	UFUNCTION(Server, Reliable)
	void Server_PlayAttackAnim(UAnimMontage* anim1P, UAnimMontage* anim3P);
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackAnim(UAnimMontage* anim1P, UAnimMontage* anim3P);

	// void MeleeAnimPlay(UAnimInstance* animInstance, UAnimMontage* anim);
	
	void OnMontageEnded(UAnimMontage* AnimMontage, bool bInterrupted);
};
