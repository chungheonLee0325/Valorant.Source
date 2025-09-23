// Fill out your copyright notice in the Description page of Project Settings.


#include "AgentBaseWidget.h"

#include "Components/TextBlock.h"
#include "Player/AgentPlayerController.h"
#include "Player/AgentPlayerState.h"
#include "Valorant/AbilitySystem/AgentAbilitySystemComponent.h"

void UAgentBaseWidget::BindToDelegatePC(UAgentAbilitySystemComponent* _asc, AAgentPlayerController* pc)
{
	pc->OnHealthChanged_PC.AddDynamic(this, &UAgentBaseWidget::UpdateDisplayHealth);
	pc->OnArmorChanged_PC.AddDynamic(this, &UAgentBaseWidget::UpdateDisplayArmor);
	pc->OnEffectSpeedChanged_PC.AddDynamic(this, &UAgentBaseWidget::UpdateDisplaySpeed);

	if (_asc == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("AgentWidget, ASC NULL"));
		return;
	}
	ASC = _asc;
}

void UAgentBaseWidget::InitUI(AAgentPlayerState* ps)
{
	// UE_LOG(LogTemp, Warning, TEXT("AgentWidget, InitUI"));
	txt_HP->SetText(FText::AsNumber(ps->GetHealth()));
	txt_Armor->SetText(FText::AsNumber(ps->GetArmor()));
}

void UAgentBaseWidget::UpdateDisplayHealth(const float health, bool bIsDamage)
{
	txt_HP->SetText(FText::AsNumber(health));
}

void UAgentBaseWidget::UpdateDisplayArmor(const float armor)
{
	txt_Armor->SetText(FText::AsNumber(armor));
}

void UAgentBaseWidget::UpdateDisplaySpeed(const float speed)
{
	txt_Speed->SetText(FText::AsNumber(speed));
}
