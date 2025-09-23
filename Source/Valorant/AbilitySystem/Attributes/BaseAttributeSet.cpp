﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Valorant.h"
#include "GameManager/SubsystemSteamManager.h"
#include "Valorant/Player/Agent/BaseAgent.h"
#include "Net/UnrealNetwork.h"

UBaseAttributeSet::UBaseAttributeSet()
{
}

void UBaseAttributeSet::ResetAttributeData()
{
	// UE_LOG(LogTemp, Warning, TEXT("UBaseAttributeSet::ResetAttributeData"));
	SetHealth(GetMaxHealth());
}

// 호출시점: SetAttributeBaseValue()처럼 수동으로 값이 바뀔 시, 속성에 변경된 값을 적용하기 직전.
// 용도: 최종적으로 값을 점검 (e.g. 최대값을 넘지 않도록 제한 / 비율 유지)
void UBaseAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetHealthAttribute())
	{
		CachedOldHealth = Health.GetCurrentValue();
	}
	
	ClampAttribute(Attribute,NewValue);
}

// 호출시점: SetBaseValue(), SetCurrentValue() 등 수동으로 값이 바뀔 시, 속성이 변경된 직후
// 용도: UI 업데이트
void UBaseAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	if (GetOwningAbilitySystemComponent()->GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	
	if (Attribute == GetHealthAttribute())
	{
		bool bIsDamage = false;
		if (OldValue > NewValue)
		{
			bIsDamage = true;
		}

		// NET_LOG(LogTemp,Error,TEXT("기존 체력: %f / 새 체력: %f / 데미지 여부: %d"), OldValue, NewValue, bIsDamage);
		
		CachedWasDamaged = bIsDamage;
		OnHealthChanged.Broadcast(NewValue, CachedWasDamaged);
		OnHealthChanged_Manual.Broadcast(NewValue, CachedWasDamaged);
		//NET_LOG(LogTemp,Warning,TEXT("OnHealthChanged_Manual %f"),Health.GetCurrentValue());
	}
	if (Attribute == GetMaxHealthAttribute())
	{
		OnMaxHealthChanged.Broadcast(NewValue);
		OnMaxHealthChanged_Manual.Broadcast(NewValue);
	}
	if (Attribute == GetArmorAttribute())
	{
		OnArmorChanged.Broadcast(NewValue);
		OnArmorChanged_Manual.Broadcast(NewValue);
	}
	if (Attribute == GetEffectSpeedMultiplierAttribute())
	{
		OnEffectSpeedChanged.Broadcast(NewValue);
		OnEffectSpeedChanged_Manual.Broadcast(NewValue);
	}
}

bool UBaseAttributeSet::PreGameplayEffectExecute(struct FGameplayEffectModCallbackData& Data)
{
	return Super::PreGameplayEffectExecute(Data);
}


// 호출시점: GE로 인해 속성이 바뀐 직후
// 용도: 이펙트, 애니메이션 등의 트리거 / 사망 처리 / 태그 조건에 따른 추가 처리
void UBaseAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		const int32 newValue = GetHealth();

		bool bIsDamage = false;
		if (CachedOldHealth > newValue)
		{
			bIsDamage = true;
		}
		
		// UE_LOG(LogTemp,Warning,TEXT("기존 체력: %f / 새 체력: %d / 데미지 여부: %d"), CachedOldHealth, newValue, bIsDamage);
		
		CachedWasDamaged = bIsDamage;
		OnHealthChanged_FromGE.Broadcast(newValue, bIsDamage);

		//NET_LOG(LogTemp,Warning,TEXT("OnHealthChanged_FromGE %f"),Health.GetCurrentValue());
	}
	else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		const int32 newValue = GetMaxHealth();
		OnMaxHealthChanged_FromGE.Broadcast(newValue);
	}
	else if (Data.EvaluatedData.Attribute == GetArmorAttribute())
	{
		const int32 newValue = GetArmor();
		OnArmorChanged_FromGE.Broadcast(newValue);
	}
	else if (Data.EvaluatedData.Attribute == GetEffectSpeedMultiplierAttribute())
	{
		const int32 newValue = GetEffectSpeedMultiplier();
		OnEffectSpeedChanged_FromGE.Broadcast(newValue);
	}
}

void UBaseAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet, Health,COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet, MaxHealth,COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet, Armor,COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet, EffectSpeedMultiplier,COND_None, REPNOTIFY_Always);
	DOREPLIFETIME(UBaseAttributeSet, CachedWasDamaged);
}

void UBaseAttributeSet::AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute,
                                                    const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty)
{
	// 최대 값이 줄었을 때, 현재 값/최대 값 비율을 유지하기 위한 계산
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();
	if (!FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) && AbilityComp)
	{
		const float CurrentValue = AffectedAttribute.GetCurrentValue();
		float NewDelta = (CurrentMaxValue > 0.f) ? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue : NewMaxValue;

		AbilityComp->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
	}
}

void UBaseAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	// ReplicatedUsing으로 인해 값이 변경되는 경우, gas에 내장되어있는 델리게이트(값의 변화를 감지하는 델리게이트)가 호출되지않음.
	// 그 델리게이트 함수가 호출될 수 있도록 연결해주는 역할을 하는 것이 아래 메서드.
	//GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseAttributeSet, Health, OldHealth);
	
	//NET_LOG(LogTemp,Warning,TEXT("UBaseAttributeSet::OnRep_Health %f"),Health.GetCurrentValue());
	OnHealthChanged.Broadcast(Health.GetCurrentValue(), CachedWasDamaged);
}

void UBaseAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	OnMaxHealthChanged.Broadcast(MaxHealth.GetCurrentValue());
}

void UBaseAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldArmor)
{
	OnArmorChanged.Broadcast(Armor.GetCurrentValue());
}

void UBaseAttributeSet::OnRep_EffectSpeed(const FGameplayAttributeData& OldEffectSpeed)
{
	OnEffectSpeedChanged.Broadcast(EffectSpeedMultiplier.GetCurrentValue());
}

void UBaseAttributeSet::ClampAttribute(const FGameplayAttribute& attribute, float& newValue) const
{
	if (attribute == GetHealthAttribute())
	{
		newValue = FMath::Clamp(newValue, 0.0f, GetMaxHealth());
	}
	else if (attribute == GetArmorAttribute())
	{
		newValue = FMath::Clamp(newValue, 0.0f, GetMaxArmor());
	}
}
