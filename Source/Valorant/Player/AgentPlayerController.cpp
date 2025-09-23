// Fill out your copyright notice in the Description page of Project Settings.


#include "AgentPlayerController.h"

#include <GameManager/SubsystemSteamManager.h>

#include "AgentPlayerState.h"
#include "Valorant.h"
#include "AbilitySystem/AgentAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BaseAttributeSet.h"
#include "Agent/BaseAgent.h"
#include "UI/MatchMap/MatchMapHUD.h"
#include "Widget/AgentBaseWidget.h"
#include "Valorant/Player/Widget/MiniMapWidget.h"
#include "Component/ShopComponent.h"
#include "Valorant/GameManager/ValorantGameInstance.h"
#include "Valorant/UI/MatchMap/MatchMapShopUI.h"


AAgentPlayerController::AAgentPlayerController()
{
	ShopComponent = CreateDefaultSubobject<UShopComponent>(TEXT("ShopComponent"));
}

void AAgentPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// UE_LOG(LogTemp, Warning, TEXT("PC, BeginPlay → %s, LocalRole=%d, IsLocal=%d"),
	// *GetName(), (int32)GetLocalRole(), IsLocalController());

	// ShopComponent 초기화
	if (ShopComponent)
	{
		// 에이전트가 가진 스킬 ID를 가져와서 상점 초기화
		// 아직 에이전트가 연결되지 않았으므로 나중에 처리
		UE_LOG(LogTemp, Display, TEXT("PC, ShopComponent found"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PC, ShopComponent not found"));
	}
	
	// GameState의 상점 닫기 이벤트 구독
	if (AMatchGameState* GameState = GetWorld()->GetGameState<AMatchGameState>())
	{
		GameState->OnShopClosed.AddDynamic(this, &AAgentPlayerController::CloseShopUI);
		GameState->OnMatchEnd.AddDynamic(this, &AAgentPlayerController::OnMatchEnd);
		GameState->OnSpikePlanted.AddDynamic(this, &AAgentPlayerController::OnSpikePlanted);
	}
}

void AAgentPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	OnRep_Pawn();
	
	InitGAS();

	if (IsLocalController())
	{
		m_GameInstance = Cast<UValorantGameInstance>(GetGameInstance());
	}

	// 에이전트 소유 시 ShopComponent 초기화
	ABaseAgent* Agent = Cast<ABaseAgent>(InPawn);
	if (Agent && ShopComponent)
	{
		AAgentPlayerState* PS = GetPlayerState<AAgentPlayerState>();
		if (PS)
		{
			int32 AgentID = PS->GetAgentID();
			UE_LOG(LogTemp, Display, TEXT("PC, Possess Agent ID: %d"), AgentID);

			// 에이전트 데이터 가져오기
			if (m_GameInstance)
			{
				FAgentData* AgentData = m_GameInstance->GetAgentData(AgentID);
				if (AgentData)
				{
					// 에이전트의 구매 가능한 능력 ID 목록 생성
					TArray<int32> SkillIDs;
					if (AgentData->AbilityID_C != 0) SkillIDs.Add(AgentData->AbilityID_C);
					if (AgentData->AbilityID_Q != 0) SkillIDs.Add(AgentData->AbilityID_Q);
					if (AgentData->AbilityID_E != 0) SkillIDs.Add(AgentData->AbilityID_E);
					if (AgentData->AbilityID_X != 0) SkillIDs.Add(AgentData->AbilityID_X);

					// 상점 초기화
					ShopComponent->InitBySkillData(SkillIDs);
					UE_LOG(LogTemp, Display, TEXT("PC, ShopComponent initialized with %d skills"), SkillIDs.Num());
				}
			}
		}
	}
}

void AAgentPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitGAS();

	m_GameInstance = Cast<UValorantGameInstance>(GetGameInstance());
	// InitAgentWidget();
}

void AAgentPlayerController::Client_EnterSpectatorMode_Implementation()
{
	StartSpectatingOnly();
}

void AAgentPlayerController::InitGAS()
{
	//한 게임당 1번 실행되도록
	if (CachedABS && CachedASC)
	{
		// UE_LOG(LogTemp, Warning, TEXT("AgentPC: already Cached"));
		return;
	}

	// NET_LOG(LogTemp, Warning, TEXT("이닛 가스"));

	if (AAgentPlayerState* ps = GetPlayerState<AAgentPlayerState>())
	{
		CachedASC = Cast<UAgentAbilitySystemComponent>(ps->GetAbilitySystemComponent());
		CachedABS = ps->GetBaseAttributeSet();
	}

	if (CachedASC == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("PC, ASC 없음"));
		return;
	}
	if (CachedABS == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("PC, ABS 없음"));
		return;
	}

	CachedABS->ResetAttributeData();
	CachedABS->OnHealthChanged.AddDynamic(this, &AAgentPlayerController::HandleHealthChanged);
	CachedABS->OnMaxHealthChanged.AddDynamic(this, &AAgentPlayerController::HandleMaxHealthChanged);
	CachedABS->OnArmorChanged.AddDynamic(this, &AAgentPlayerController::HandleArmorChanged);
	CachedABS->OnEffectSpeedChanged.AddDynamic(this, &AAgentPlayerController::HandleEffectSpeedChanged);
}

void AAgentPlayerController::InitAgentWidget()
{
	if (AgentWidgetClass == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerController에 AgentWidget 좀 넣어주세요."));
		return;
	}

	if (!AgentWidget)
	{
		AgentWidget = CreateWidget<UMatchMapHUD>(this, AgentWidgetClass);
		AgentWidget->AddToViewport();
		AgentWidget->BindToDelegatePC(CachedASC, this);
	}

	// 크레딧 UI 바인딩
	BindCreditWidgetDelegate();

	if (AAgentPlayerState* ps = GetPlayerState<AAgentPlayerState>())
	{
		AgentWidget->InitUI(ps);
	}
}

void AAgentPlayerController::HandleHealthChanged(float NewHealth, bool bIsDamage)
{
	// NET_LOG(LogTemp,Display,TEXT("PC, Health Changed 데미지 여부: %d"), bIsDamage);
	OnHealthChanged_PC.Broadcast(NewHealth, bIsDamage);
}

void AAgentPlayerController::OnKillEvent(ABaseAgent* InstigatorAgent, ABaseAgent* VictimAgent,
	const FKillFeedInfo& Info)
{
	//NET_LOG(LogTemp, Warning, TEXT("%hs Called, InstigatorAgentName: %s"), __FUNCTION__, *InstigatorAgent->GetName());
	OnKillEvent_PC.Broadcast(InstigatorAgent, VictimAgent, Info);
}

void AAgentPlayerController::HandleMaxHealthChanged(float NewMaxHealth)
{
	//UE_LOG(LogTemp,Display,TEXT("PC, MaxHealth Changed"));
	OnMaxHealthChanged_PC.Broadcast(NewMaxHealth);
}

void AAgentPlayerController::HandleArmorChanged(float NewArmor)
{
	//UE_LOG(LogTemp,Display,TEXT("PC, Armor Changed"));
	OnArmorChanged_PC.Broadcast(NewArmor);
}

void AAgentPlayerController::HandleEffectSpeedChanged(float NewSpeed)
{
	//UE_LOG(LogTemp,Display,TEXT("PC, MoveSpeed Changed"));
	OnEffectSpeedChanged_PC.Broadcast(NewSpeed);
}

void AAgentPlayerController::NotifyChangedAmmo(const bool bDisplayWidget, const int MagazineAmmo, const int SpareAmmo) const
{
	OnChangedAmmo.Broadcast(bDisplayWidget, MagazineAmmo, SpareAmmo);
}

void AAgentPlayerController::RequestPurchaseAbility(int AbilityID)
{
	if (AbilityID != 0)
	{
		Server_RequestPurchaseAbility(AbilityID);
	}
}

void AAgentPlayerController::Server_RequestPurchaseAbility_Implementation(int AbilityID)
{
	AAgentPlayerState* PS = GetPlayerState<AAgentPlayerState>();
	if (PS)
	{
		if (ShopComponent)
		{
			// ShopComponent를 통해 어빌리티 구매 시도
			ShopComponent->PurchaseAbility(AbilityID);
		}
	}
}

bool AAgentPlayerController::Server_RequestPurchaseAbility_Validate(int AbilityID)
{
	// ToDo 유효성 검사 - 해당 케릭의 스킬 목록이 맞는지...
	return AbilityID != 0;
}

void AAgentPlayerController::RequestShopUI()
{
	if (ShopComponent)
	{
		// 이미 UI가 열려있는지 확인
		bool bIsShopUIOpen = (ShopUI != nullptr && ShopUI->IsVisible());
		
		if (bIsShopUIOpen)
		{
			// UI가 열려있으면 닫기
			RequestCloseShopUI();
		}
		else
		{
			// UI가 닫혀있으면 열기
			RequestOpenShopUI();
		}
	}
}

void AAgentPlayerController::RequestOpenShopUI()
{
	if (ShopComponent)
	{
		// 상점 활성화 요청
		ShopComponent->SetShopActive(true);

		// 상점 UI 표시
		OpenShopUI();
	}
}

void AAgentPlayerController::RequestCloseShopUI()
{
	if (ShopComponent)
	{
		// 상점 비활성화 요청
		ShopComponent->SetShopActive(false);

		// 상점 UI 닫기
		CloseShopUI();
	}
}

void AAgentPlayerController::ClientRPC_SaveMatchResult_Implementation(const FMatchDTO& MatchDto,
	const TArray<FPlayerMatchDTO>& PlayerMatchDtoArray)
{
	if (auto* GI = GetGameInstance<UValorantGameInstance>())
	{
		GI->SaveMatchResult(MatchDto, PlayerMatchDtoArray);
	}
	
	if (auto* SubsystemManager = GetGameInstance()->GetSubsystem<USubsystemSteamManager>())
	{
		SubsystemManager->DestroySession();
	}

	if (false == HasAuthority())
	{
		ClientTravel("/Game/Maps/MainMap", TRAVEL_Absolute);
	}
}

void AAgentPlayerController::OpenShopUI()
{
	// 이미 UI가 열려 있으면 다시 열지 않음
	if (ShopUI)
	{
		ShopUI->SetVisibility(ESlateVisibility::Visible);

		// 입력 모드 설정 (UI에 포커스)
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(ShopUI->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);

		// 마우스 커서 표시
		bShowMouseCursor = true;

		return;
	}

	// UI 클래스가 설정되어 있는지 확인
	if (ShopUIClass == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("상점 UI 클래스가 설정되지 않았습니다."));
		return;
	}

	// 상점 UI 생성
	ShopUI = CreateWidget<UMatchMapShopUI>(this, ShopUIClass);
	if (ShopUI)
	{
		// UI 초기화 및 표시
		ShopUI->InitializeShopUI(this);
		ShopUI->AddToViewport(10); // z-order 10 (다른 UI보다 위에 표시)

		// 입력 모드 설정 (UI에 포커스)
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(ShopUI->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);

		// 마우스 커서 표시
		bShowMouseCursor = true;

		// 상점 컴포넌트의 이벤트에 UI 업데이트 연결
		if (ShopComponent)
		{
			// 상점 가용성 변경 이벤트에 UI 갱신 함수 연결
			ShopComponent->OnShopAvailabilityChanged.AddDynamic(
				this, &AAgentPlayerController::OnShopAvailabilityChanged);
		}
	}
}

void AAgentPlayerController::CloseShopUI()
{
	if (ShopUI)
	{
		// UI 숨기기 또는 제거
		//ShopUI->SetVisibility(ESlateVisibility::Collapsed);
		ShopUI->RemoveFromParent(); // 완전히 제거하려면 이 라인 사용
		ShopUI = nullptr; // 참조 제거

		// 입력 모드를 게임으로 되돌리기
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);

		// 마우스 커서 숨기기
		bShowMouseCursor = false;

		ShopComponent->OnShopAvailabilityChanged.RemoveAll(this);
	}
}

// 상점 가용성 변경 시 호출될 이벤트 핸들러
void AAgentPlayerController::OnShopAvailabilityChanged()
{
	if (ShopComponent)
	{
		// 상점이 비활성화되면 UI도 닫기
		if (!ShopComponent->IsShopActive() && ShopUI)
		{
			CloseShopUI();
		}
		// 상점이 활성화되고 UI가 없으면 열기
		else if (ShopComponent->IsShopActive() && !ShopUI)
		{
			OpenShopUI();
		}
	}
}

void AAgentPlayerController::RequestPurchaseWeapon(int32 WeaponID)
{
	if (WeaponID != 0)
	{
		Server_RequestPurchaseWeapon(WeaponID);
	}
}

void AAgentPlayerController::Server_RequestPurchaseWeapon_Implementation(int32 WeaponID)
{
	AAgentPlayerState* PS = GetPlayerState<AAgentPlayerState>();
	if (PS)
	{
		if (ShopComponent)
		{
			// 무기 구매 시도
			ShopComponent->PurchaseWeapon(WeaponID);
		}
	}
}

bool AAgentPlayerController::Server_RequestPurchaseWeapon_Validate(int32 WeaponID)
{
	return WeaponID != 0;
}

void AAgentPlayerController::RequestPurchaseArmor(int32 ArmorLevel)
{
	if (ArmorLevel > 0)
	{
		Server_RequestPurchaseArmor(ArmorLevel);
	}
}

void AAgentPlayerController::Server_RequestPurchaseArmor_Implementation(int32 ArmorID)
{
	AAgentPlayerState* PS = GetPlayerState<AAgentPlayerState>();
	if (PS)
	{
		if (ShopComponent)
		{
			// 방어구 구매 시도
			ShopComponent->PurchaseArmor(ArmorID);
		}
	}
}

bool AAgentPlayerController::Server_RequestPurchaseArmor_Validate(int32 ArmorID)
{
	return ArmorID > 0;
}

void AAgentPlayerController::BindCreditWidgetDelegate()
{
	// AgentBaseWidget에는 크레딧 기능 필요 없음
	// 기존 크레딧 관련 바인딩 제거

	// AAgentPlayerState* PS = GetPlayerState<AAgentPlayerState>();
	// if (!PS)
	// {
	// 	UE_LOG(LogTemp, Error, TEXT("PlayerState가 없습니다."));
	// 	return;
	// }
	// 
	// // 크레딧 UI 바인딩 - PlayerState의 델리게이트 사용
	// PS->OnCreditChangedDelegate.AddDynamic(AgentWidget, &UMatchMapHUD::UpdateCreditDisplay);
	// 
	// // 초기 크레딧 값으로 UI 업데이트
	// AgentWidget->UpdateCreditDisplay(PS->GetCurrentCredit());

	// 이 함수는 더 이상 크레딧 바인딩에 사용되지 않음
	// 필요한 경우 다른 UI 요소 바인딩에 활용
}

void AAgentPlayerController::Client_ReceivePurchaseResult_Implementation(
	bool bSuccess, int32 ItemID, EShopItemType ItemType, const FString& FailureReason)
{
	OnServerPurchaseResult.Broadcast(bSuccess, ItemID, ItemType, FailureReason);
}

void AAgentPlayerController::HideFollowUpInputUI()
{
	Client_HideFollowUpInputUI();
}

void AAgentPlayerController::Client_HideFollowUpInputUI_Implementation()
{
	if (auto* MatchMapHud = Cast<UMatchMapHUD>(GetMatchMapHud()))
	{
		MatchMapHud->HideFollowUpInputUI();
	}
}

//ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ
//             CYT             ♣
//ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ

// 미니맵 초기화 함수
void AAgentPlayerController::InitializeMinimap()
{
	// 미니맵 위젯 클래스가 설정되어 있는지 확인
	if (MinimapWidgetClass)
	{
		// 미니맵 위젯 생성
		MinimapWidget = CreateWidget<UMiniMapWidget>(this, MinimapWidgetClass);
		if (MinimapWidget)
		{
			// 뷰포트에 위젯 추가
			MinimapWidget->AddToViewport(1); // Z-Order 1로 설정 (UI 레이어)
            
			NET_LOG(LogTemp, Warning, TEXT("%hs Called, 미니맵 위젯이 생성되었습니다."), __FUNCTION__);
            
			// 미니맵 위젯 생성 후 에이전트 스캔은 위젯 내부에서 자동으로 수행됨
		}
		else
		{
			NET_LOG(LogTemp, Error, TEXT("%hs Called, 미니맵 위젯 생성 실패!"), __FUNCTION__);
		}
	}
	else
	{
		NET_LOG(LogTemp, Error, TEXT("%hs Called, MinimapWidgetClass가 설정되지 않았습니다!"), __FUNCTION__);
	}
}

void AAgentPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	
	//ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ
	//             CYT             ♣
	//ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ
	
	// 로컬 플레이어만 미니맵 생성 (멀티플레이어 최적화)
	// 현재 컨트롤러가 로컬 플레이어의 것인지 확인
	if (IsLocalPlayerController() && nullptr == MinimapWidget)
	{
		// 미니맵 초기화 함수 호출
		InitializeMinimap();
	}

	if (nullptr != GetPawn())
	{
		if (auto* Agent = Cast<ABaseAgent>(GetPawn()))
		{
			Agent->OnAgentDamaged.RemoveDynamic(this, &AAgentPlayerController::OnDamaged);
			Agent->OnAgentDamaged.AddDynamic(this, &AAgentPlayerController::OnDamaged);
			Agent->OnAgentHealed.RemoveDynamic(this,&AAgentPlayerController::OnHealed);
			Agent->OnAgentHealed.AddDynamic(this,&AAgentPlayerController::OnHealed);
			Agent->OnSwitchEquipment.RemoveDynamic(this,&AAgentPlayerController::OnSwitchWeapon);
			Agent->OnSwitchEquipment.AddDynamic(this,&AAgentPlayerController::OnSwitchWeapon);
			Agent->OnSpikeOwnChanged.RemoveDynamic(this,&AAgentPlayerController::OnSpikeOwnChanged);
			Agent->OnSpikeOwnChanged.AddDynamic(this,&AAgentPlayerController::OnSpikeOwnChanged);
		}
	}
}

void AAgentPlayerController::OnMatchEnd(const bool bBlueWin)
{
	if (auto* MatchMapHud = Cast<UMatchMapHUD>(GetMatchMapHud()))
	{
		const auto* PS = GetPlayerState<AAgentPlayerState>();
		if (PS)
		{
			const bool bWin = !(PS->bIsBlueTeam ^ bBlueWin);
			// NOT(XOR) 하면 아래 결과가 나옴
			// True, True = Win
			// True, False = False
			// False, True = False
			// False, False = True
			MatchMapHud->OnMatchEnd(bWin);
		}
		else
		{
			NET_LOG(LogTemp, Error, TEXT("%hs Called, PS is nullptr"), __FUNCTION__);
		}
	}
	else
	{
		NET_LOG(LogTemp, Error, TEXT("%hs Called, AgentWidget is nullptr"), __FUNCTION__);
	}
}

void AAgentPlayerController::OnDamaged(const FVector& HitOrg, const EAgentDamagedPart AgentDamagedPart,
	const EAgentDamagedDirection AgentDamagedDirection, const bool bArg, const bool bCond, const bool bLowState)
{
	OnDamaged_PC.Broadcast(HitOrg, AgentDamagedPart, AgentDamagedDirection, bArg, bCond, bLowState);
}

void AAgentPlayerController::OnHealed(const bool bHighState)
{
	OnHealed_PC.Broadcast(bHighState);
}

void AAgentPlayerController::OnSpikePlanted(AMatchPlayerController* Planter)
{
	if (auto* MatchMapHud = Cast<UMatchMapHUD>(GetMatchMapHud()))
	{
		MatchMapHud->SpikePlanted();
	}
}

void AAgentPlayerController::OnSwitchWeapon(const EInteractorType EquipmentState)
{
	if (auto* MatchMapHud = Cast<UMatchMapHUD>(GetMatchMapHud()))
	{
		MatchMapHud->OnSwitchWeapon(EquipmentState);
	}
}

void AAgentPlayerController::OnSpikeOwnChanged(const bool bOwnSpike)
{
	if (auto* MatchMapHud = Cast<UMatchMapHUD>(GetMatchMapHud()))
	{
		MatchMapHud->OnSpikeOwnChanged(bOwnSpike);
	}
}
