﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "ValorantGameInstance.h"

#include "MoviePlayer.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "SubsystemSteamManager.h"
#include "Valorant.h"
#include "AbilitySystem/ValorantGameplayTags.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h" 
#include "ResourceManager/ValorantGameType.h"
#include "UI/MainMap/MainMapCoreUI.h"
#include "UI/MainMap/MainMapMenuUI.h"
#include "UI/MainMap/MatchResultPage.h"
#include "Web/DatabaseManager.h"

template<typename RowStructType, typename IDType>
void LoadDataTableToMap(const FString& Path, TMap<IDType, RowStructType>& OutMap, IDType RowStructType::* IDMember)
{
	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *Path);
	if (DataTable)
	{
		TArray<FName> RowNames = DataTable->GetRowNames();
		for (const FName& RowName : RowNames)
		{
			RowStructType* Row = DataTable->FindRow<RowStructType>(RowName, TEXT(""));
			if (Row != nullptr)
			{
				OutMap.Add(Row->*IDMember, *Row);
			}
		}
	}
}

void UValorantGameInstance::Init()
{
	Super::Init();
	FValorantGameplayTags::Get().InitializeNativeTags();
	
	/*
	 *	LoadingScreen
	 */
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UValorantGameInstance::BeginLoadingScreen);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UValorantGameInstance::EndLoadingScreen);

	/*
	 *	DataTable
	 */
	// Agent Data Load
	LoadDataTableToMap<FAgentData, int32>(
		TEXT("/Script/Engine.DataTable'/Game/BluePrint/DataTable/dt_Agent.dt_Agent'"),
		dt_Agent,
		&FAgentData::AgentID
	);

	// Weapon Data Load
	LoadDataTableToMap<FWeaponData, int32>(
		TEXT("/Script/Engine.DataTable'/Game/BluePrint/DataTable/dt_Weapon.dt_Weapon'"),
		dt_Weapon,
		&FWeaponData::WeaponID
	);

	// Agent Data Load
	LoadDataTableToMap<FAbilityData, int32>(
		TEXT("/Script/Engine.DataTable'/Game/BluePrint/DataTable/dt_Ability.dt_Ability'"),
		dt_Ability,
		&FAbilityData::AbilityID
	);

	/*
	 *	OnlineSubsystem
	 */
	const IOnlineSessionPtr SessionInterface = USubsystemSteamManager::GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		const auto* OnlineSubsystem = Online::GetSubsystem(GetWorld());
		if (OnlineSubsystem)
		{
			const FName SubsystemName = OnlineSubsystem->GetSubsystemName();
			UE_LOG(LogTemp, Warning, TEXT("SubsystemName : %s"), *SubsystemName.ToString());

			const IOnlineIdentityPtr IdentityInterface = OnlineSubsystem->GetIdentityInterface();
			if (IdentityInterface.IsValid())
			{
				const auto UniquePlayerId = IdentityInterface->GetUniquePlayerId(0);
				const FString Nickname = IdentityInterface->GetPlayerNickname(0);
				const auto LoginStatus = IdentityInterface->GetLoginStatus(0);
				UE_LOG(LogTemp, Warning, TEXT("UniquePlayerId : %s"), *UniquePlayerId->ToString());
				UE_LOG(LogTemp, Warning, TEXT("Nickname : %s"), *Nickname);
				UE_LOG(LogTemp, Warning, TEXT("LoginStatus : %s"), ELoginStatus::ToString(LoginStatus));
				// PlayerId = UniquePlayerId->ToString();
				PlayerId = Nickname;
				Platform = SubsystemName.ToString();
				OnGetPlayerCompletedDelegate.AddDynamic(this, &UValorantGameInstance::OnGetPlayerCompleted);
				// TODO: UniquePlayerId로 해야 함
				// UDatabaseManager::GetInstance()->GetPlayer(UniquePlayerId->ToString(), SubsystemName.ToString(), OnGetPlayerCompletedDelegate);
				UDatabaseManager::GetInstance()->GetPlayer(Nickname, SubsystemName.ToString(), OnGetPlayerCompletedDelegate);
			}
		}
	}
}

void UValorantGameInstance::BeginLoadingScreen(const FString& MapName)
{
	UE_LOG(LogTemp, Warning, TEXT("BeginLoadingScreen MapName: %s"), *MapName);
	if (!IsRunningDedicatedServer())
	{
		// 로딩하는 맵 종류에 따라 다른 UI를 표시한다.
		TSubclassOf<UUserWidget> WidgetClass;
		if (MapName.Contains(TEXT("MatchMap")))
		{
			WidgetClass = LobbyToSelectLoadingWidgetClass;
		}
		else
		{
			WidgetClass = GameStartUpLoadingWidgetClass;
		}
		
		if (CurrentLoadingWidget)
		{
			CurrentLoadingWidget->RemoveFromParent();
			CurrentLoadingWidget = nullptr;
		}
		
		CurrentLoadingWidget = CreateWidget<UUserWidget>(this, WidgetClass);
		if (CurrentLoadingWidget)
		{
			FLoadingScreenAttributes LoadingScreen;
			LoadingScreen.WidgetLoadingScreen = CurrentLoadingWidget->TakeWidget();
			LoadingScreen.bAllowInEarlyStartup = false;
			LoadingScreen.PlaybackType = MT_Normal;
			LoadingScreen.bAllowEngineTick = false;
			LoadingScreen.bWaitForManualStop = false;
			LoadingScreen.bAutoCompleteWhenLoadingCompletes = true;
			// 최소한 3초는 표시한다.
			LoadingScreen.MinimumLoadingScreenDisplayTime = 3.f;

			CurrentLoadingWidget = nullptr;
			GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);
		}
	}
}

void UValorantGameInstance::EndLoadingScreen(UWorld* InLoadedWorld)
{
	UE_LOG(LogTemp, Warning, TEXT("EndLoadingScreen"));
	
	if (CurrentLoadingWidget)
	{
		CurrentLoadingWidget->RemoveFromParent();
		CurrentLoadingWidget = nullptr;
	}

	if (GetWorld()->GetNetMode() == NM_ListenServer)
	{
		if (InLoadedWorld->GetMapName().Contains(TEXT("MatchMap")))
		{
			CurrentLoadingWidget = CreateWidget<UUserWidget>(this, LobbyToSelectLoadingWidgetClass);
			if (CurrentLoadingWidget)
			{
				CurrentLoadingWidget->AddToViewport();
			}
		}
	}

	if (InLoadedWorld->GetMapName().Contains(TEXT("MainMap")) && PlayerMatchResultArray.Num() > 0)
	{
		auto* Widget = CreateWidget<UMatchResultPage>(this, MatchResultPageClass);
		for (const auto& PlayerMatchInfo : PlayerMatchResultArray)
		{
			Widget->DisplayMatchResult(MatchResult);
			Widget->AddRow(PlayerMatchInfo);
		}
		Widget->AddToViewport(100000);
	}
}

void UValorantGameInstance::OnMatchHasStarted()
{
	NET_LOG(LogTemp, Warning, TEXT("%hs Called"), __FUNCTION__);
	if (CurrentLoadingWidget)
	{
		CurrentLoadingWidget->RemoveFromParent();
		CurrentLoadingWidget = nullptr;
	}
}

void UValorantGameInstance::SaveMatchResult(const FMatchDTO& MatchDto,
	const TArray<FPlayerMatchDTO>& PlayerMatchDtoArray)
{
	MatchResult = MatchDto;
	PlayerMatchResultArray = PlayerMatchDtoArray;
}

void UValorantGameInstance::OnStart()
{
	Super::OnStart();
	if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
	{
		Settings->SetOverallScalabilityLevel(1);
		Settings->ApplySettings(false);
	}
}

void UValorantGameInstance::OnGetPlayerCompleted(const bool bIsSuccess, const FPlayerDTO& PlayerDto)
{
	if (bIsSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnGetPlayerCompleted, PlayerId: %s, Platform: %s"), *PlayerDto.player_id, *PlayerDto.platform);
	}
	else
	{
		UDatabaseManager::GetInstance()->PostPlayer(PlayerId, Platform);
	}
}

FAgentData* UValorantGameInstance::GetAgentData(int AgentID)
{
	if (FAgentData* data = dt_Agent.Find(AgentID))
	{
		return data;
	}
	UE_LOG(LogTemp, Error, TEXT("해당 ID의 데이터를 로드할 수 없습니다. AgentID:%d"),AgentID);
	return nullptr;
}

FWeaponData* UValorantGameInstance::GetWeaponData(int WeaponID)
{
	if (FWeaponData* data = dt_Weapon.Find(WeaponID))
	{
		return data;
	}
	UE_LOG(LogTemp, Error, TEXT("해당 ID의 데이터를 로드할 수 없습니다. WeaponID:%d"),WeaponID);
	return nullptr;
}

void UValorantGameInstance::GetAllWeaponData(TArray<FWeaponData*>& WeaponList)
{
	for (auto& pair :dt_Weapon)
	{
		WeaponList.Add(&pair.Value);
	}
}

FGameplayEffectData* UValorantGameInstance::GetGEffectData(int GEffectID)
{
	if (FGameplayEffectData* data = dt_GEffect.Find(GEffectID))
	{
		return data;
	}
	UE_LOG(LogTemp, Error, TEXT("해당 ID의 데이터를 로드할 수 없습니다. GEffectID:%d"),GEffectID);
	return nullptr;
}

FAbilityData* UValorantGameInstance::GetAbilityData(int AbilityID)
{
	if (FAbilityData* data = dt_Ability.Find(AbilityID))
	{
		return data;
	}
	UE_LOG(LogTemp, Error, TEXT("해당 ID의 데이터를 로드할 수 없습니다. AbilityID:%d"),AbilityID);
	return nullptr;
}

UTexture2D* UValorantGameInstance::GetAgentIcon(int AgentID)
{
	if (const auto* Data = GetAgentData(AgentID))
	{
		return Data->AgentIcon;
	}
	return nullptr;
}

UValorantGameInstance* UValorantGameInstance::Get(class UWorld* World)
{
	return Cast<UValorantGameInstance>(World->GetGameInstance());
}
