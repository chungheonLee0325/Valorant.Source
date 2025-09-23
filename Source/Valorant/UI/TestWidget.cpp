﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "TestWidget.h"

#include "Web/DatabaseManager.h"

void UTestWidget::NativeConstruct()
{
	Super::NativeConstruct();
	DatabaseManager = UDatabaseManager::GetInstance();
	OnGetMatchCompletedDelegate.AddDynamic(this, &ThisClass::OnGetMatchCompleted);
	OnPostMatchCompletedDelegate.AddDynamic(this, &ThisClass::OnPostMatchCompleted);
	OnGetPlayerMatchCompletedDelegate.AddDynamic(this, &ThisClass::OnGetPlayerMatchCompleted);
}

void UTestWidget::OnGetMatchCompleted(const bool bIsSuccess, const FMatchDTO& MatchDto)
{
	if (bIsSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs Called, blue_score: %d, red_score: %d"), __FUNCTION__, MatchDto.blue_score, MatchDto.red_score);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs Called, Failed"), __FUNCTION__);
	}
}

void UTestWidget::OnPostMatchCompleted(const bool bIsSuccess, const FMatchDTO& CreatedMatchDto)
{
	if (bIsSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs Called, match_id: %d, start_timestamp: %s"), __FUNCTION__, CreatedMatchDto.match_id, *CreatedMatchDto.start_timestamp);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs Called, Failed"), __FUNCTION__);
	}
}

void UTestWidget::OnGetPlayerMatchCompleted(const bool bIsSuccess, const FPlayerMatchDTO& PlayerMatchDto)
{
	if (bIsSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs Called, player_id: %s, match_id: %d"), __FUNCTION__, *PlayerMatchDto.player_id, PlayerMatchDto.match_id);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs Called, Failed"), __FUNCTION__);
	}
}

void UTestWidget::GetPlayer()
{
}

void UTestWidget::PostPlayer()
{
}

void UTestWidget::PutPlayer()
{
}

void UTestWidget::GetMatch()
{
	DatabaseManager->GetMatch(7, OnGetMatchCompletedDelegate);
}

void UTestWidget::PostMatch()
{
	DatabaseManager->PostMatch(OnPostMatchCompletedDelegate);
}

void UTestWidget::PutMatch()
{
	FMatchDTO MatchDto;
	MatchDto.map_id = 999;
	MatchDto.blue_score = 100;
	MatchDto.red_score = 200;
	DatabaseManager->PutMatch(MatchDto);
}

void UTestWidget::GetPlayerMatch()
{
	DatabaseManager->GetPlayerMatch(TEXT("test"), 3, OnGetPlayerMatchCompletedDelegate);
}

void UTestWidget::PostPlayerMatch()
{
	FPlayerMatchDTO PlayerMatchDto;
	PlayerMatchDto.player_id = TEXT("unreal");
	PlayerMatchDto.match_id = 999;
	DatabaseManager->PostPlayerMatch(PlayerMatchDto);
}
