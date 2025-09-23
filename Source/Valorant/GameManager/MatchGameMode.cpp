﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "MatchGameMode.h"

#include "MatchGameState.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "SubsystemSteamManager.h"
#include "Valorant.h"
#include "AbilitySystem/AgentAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BaseAttributeSet.h"
#include "GameFramework/PlayerStart.h"
#include "GameManager/ValorantGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Player/AgentPlayerController.h"
#include "Player/AgentPlayerState.h"
#include "Player/MatchPlayerController.h"
#include "Player/MatchPlayerState.h"
#include "Player/Agent/BaseAgent.h"
#include "Player/Component/CreditComponent.h"
#include "Player/Widget/MiniMapWidget.h"
#include "ValorantObject/Spike/Spike.h"
#include "Weapon/BaseWeapon.h"

AMatchGameMode::AMatchGameMode()
{
#ifdef DEBUGTEST
	MaxTime = 0.0f;
	RemainRoundStateTime = 0.0f;
	SelectAgentTime = 60.0f;
	PreRoundTime = 15.0f; // org: 45.0f
	BuyPhaseTime = 15.0f; // org: 30.0f
	InRoundTime = 50.0f; // org: 100.0f
	EndPhaseTime = 5.0f; // org: 10.0f
	SpikeActiveTime = 25.0f; // org: 45.0f
	bReadyToEndMatch = false;
	LeavingMatchTime = 10.0f;
#endif
}

bool AMatchGameMode::IsAttacker(const bool bIsBlueTeam) const
{
	return bIsBlueTeam ? !IsShifted() : IsShifted();
}

bool AMatchGameMode::IsShifted() const
{
	// UE_LOG(LogTemp, Warning, TEXT("%hs Called, CurrentRound: %d >= ShiftRound: %d"), __FUNCTION__, CurrentRound, ShiftRound);
	return CurrentRound >= ShiftRound;
}

void AMatchGameMode::BeginPlay()
{
	Super::BeginPlay();

	ValorantGameInstance = Cast<UValorantGameInstance>(GetGameInstance());

	SubsystemManager = GetGameInstance()->GetSubsystem<USubsystemSteamManager>();
	if (SubsystemManager == nullptr)
	{
		NET_LOG(LogTemp, Warning, TEXT("%hs Called, SubsystemManager is nullptr"), __FUNCTION__);
		return;
	}

	const IOnlineSessionPtr SessionInterface = SubsystemManager->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		NET_LOG(LogTemp, Warning, TEXT("%hs Called, SessionInterface is not valid"), __FUNCTION__);
		return;
	}
	auto* Session = SubsystemManager->GetNamedOnlineSession();
	if (nullptr == Session)
	{
		NET_LOG(LogTemp, Warning, TEXT("%hs Called, Session is nullptr"), __FUNCTION__);
	}
	else
	{
		Session->SessionSettings.Set(FName("bReadyToTravel"), true,
		                             EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		bool bSuccess = SessionInterface->UpdateSession(NAME_GameSession, Session->SessionSettings, true);
		if (bSuccess)
		{
			NET_LOG(LogTemp, Warning, TEXT("%hs Called, Try UpdateSession Completed"), __FUNCTION__);	
		}
		else
		{
			NET_LOG(LogTemp, Error, TEXT("%hs Called, Try UpdateSession Failed"), __FUNCTION__);
		}
	}

	OnPostMatchCompletedDelegate.AddDynamic(this, &AMatchGameMode::OnPostMatchCompleted);
	UDatabaseManager::GetInstance()->PostMatch(OnPostMatchCompletedDelegate);

	RequiredPlayerCount = SubsystemManager->ReqMatchAutoStartPlayerCount;
}

void AMatchGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// InRound상태일 때 종료 조건인지 확인
	// 굳이 Tick에서 확인하는 이유는 동시 전멸 체크를 위함임.
	if (RoundSubState == ERoundSubState::RSS_InRound)
	{
		if (TeamBlueRemainingAgentNum <= 0 && TeamRedRemainingAgentNum <= 0)
		{
			// 동시에 전멸하면 블루팀이 이기는가? (선공: 블루)
			// 스파이크설치X && 교대X -> 패배
			// 스파이크설치O && 교대X -> 승리
			// 스파이크설치X && 교대O -> 승리
			// 스파이크설치O && 교대O -> 패배
			// 스파이크설치와 교대 bool값이 서로 다르면 승리 -> XOR연산
			StartEndPhaseByEliminated(bSpikePlanted ^ !IsShifted());
		}
		else if (TeamBlueRemainingAgentNum <= 0)
		{
			//모두 제거된 쪽이 공격이고, 스파이크가 설치되어 있다면 게임 끝내지 않음
			if (bSpikePlanted == true && !IsShifted())
			{
				return;
			}
			StartEndPhaseByEliminated(false);
		}
		else if (TeamRedRemainingAgentNum <= 0)
		{
			//모두 제거된 쪽이 공격이고, 스파이크가 설치되어 있다면 게임 끝내지 않음
			if (bSpikePlanted == true && IsShifted())
			{
				return;
			}
			StartEndPhaseByEliminated(true);
		}
	}

	RemainRoundStateTime = FMath::Clamp(RemainRoundStateTime - DeltaSeconds, 0.0f, MaxTime);

	AMatchGameState* MatchGameState = GetGameState<AMatchGameState>();
	if (MatchGameState)
	{
		MatchGameState->SetRemainRoundStateTime(RemainRoundStateTime);
	}
}

void AMatchGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
                              FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	NET_LOG(LogTemp, Warning,
	        TEXT("AMainMenuGameMode::PreLogin Options: %s, Address: %s, UniqueId: %s, ErrorMessage: %s"), *Options,
	        *Address, *(UniqueId.IsValid() ? UniqueId->ToString() : FString(TEXT("INVALID"))), *ErrorMessage);
}

void AMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	const auto Address = NewPlayer->GetPlayerNetworkAddress();
	const auto UniqueId = NewPlayer->GetUniqueID();
	NET_LOG(LogTemp, Warning, TEXT("AMainMenuGameMode::PostLogin Address: %s, UniqueId: %d"), *Address, UniqueId);
	auto* Controller = Cast<AMatchPlayerController>(NewPlayer);
	Controller->SetGameMode(this);
	
	NotifyGameStart(Controller, true);
}

void AMatchGameMode::OnPostMatchCompleted(const bool bIsSuccess, const FMatchDTO& CreatedMatchDto)
{
	CurrentMatchInfo = MakeShared<FMatchDTO>(CreatedMatchDto);
}

void AMatchGameMode::OnControllerBeginPlay(AMatchPlayerController* Controller, const FString& Nickname, const FString& RealNickname)
{
	auto* AgentPC = Cast<AAgentPlayerController>(Controller);

	const bool bIsBlueTeam =  RequiredPlayerCount / 2 > LoggedInPlayerNum;
	const bool bIsAttacker = bIsBlueTeam;
	
	FPlayerMatchDTO PlayerMatchInfo;
	PlayerMatchInfo.player_id = RealNickname;
	PlayerMatchInfo.team = bIsBlueTeam ? 0 : 1;
	PlayerMatchInfoMap.Add(Cast<AAgentPlayerController>(Controller), PlayerMatchInfo);

	FPlayerDTO PlayerDto;
	PlayerDto.player_id = RealNickname;
	PlayerMap.Add(Cast<AAgentPlayerController>(Controller), PlayerDto);
	
	auto* PlayerState = Controller->GetPlayerState<AMatchPlayerState>();
	if (PlayerState)
	{
		PlayerState->bIsBlueTeam = bIsBlueTeam;
		PlayerState->bIsAttacker = bIsAttacker;
		PlayerState->DisplayName = Nickname;
	}

	// TODO: 삭제 예정
	FMatchPlayer PlayerInfo;
	PlayerInfo.Controller = AgentPC;
	PlayerInfo.Nickname = Nickname;
	PlayerInfo.bIsBlueTeam = bIsBlueTeam;
	MatchPlayers.Add(PlayerInfo);
	if (PlayerInfo.bIsBlueTeam)
	{
		BlueTeamPlayerNameArray.Add(Nickname);
	}
	else
	{
		RedTeamPlayerNameArray.Add(Nickname);
	}
	
	++LoggedInPlayerNum;
	NET_LOG(LogTemp, Warning, TEXT("%hs Called, Nickname: %s, bIsBlueTeam: %hs"), __FUNCTION__, *Nickname, PlayerState->bIsBlueTeam?"True":"False");
}

void AMatchGameMode::OnLockIn(AMatchPlayerController* Player, int AgentId)
{
	auto* PS = Player->GetPlayerState<AMatchPlayerState>();
	if (nullptr == PS)
	{
		NET_LOG(LogTemp, Warning, TEXT("%hs Called, PS is nullptr"), __FUNCTION__);
		return;
	}

	bool bIsBlueTeam = PS->bIsBlueTeam;
	for (const auto& PlayerInfo : MatchPlayers)
	{
		if (bIsBlueTeam == PlayerInfo.bIsBlueTeam)
		{
			PlayerInfo.Controller->ClientRPC_OnLockIn(PS->DisplayName, AgentId);
		}
	}

	if (auto* agentPS = Player->GetPlayerState<AAgentPlayerState>())
	{
		agentPS->MulticastRPC_SetAgentID(AgentId);
	}

	if (++LockedInPlayerNum >= RequiredPlayerCount)
	{
		NET_LOG(LogTemp, Warning, TEXT("%hs Called, All Players Completed Lock In"), __FUNCTION__);
		GetWorld()->GetTimerManager().ClearTimer(RoundTimerHandle);
		RemainRoundStateTime = 5.0f;
		AMatchGameState* MatchGameState = GetGameState<AMatchGameState>();
		if (MatchGameState)
		{
			MatchGameState->SetRemainRoundStateTime(RemainRoundStateTime);
		}
		GetWorld()->GetTimerManager().SetTimer(RoundTimerHandle, this, &AMatchGameMode::StartPreRound, RemainRoundStateTime);
	}
}

void AMatchGameMode::OnAgentSelected(AMatchPlayerController* MatchPlayerController, int SelectedAgentID)
{
	auto* PS = MatchPlayerController->GetPlayerState<AMatchPlayerState>();
	if (nullptr == PS)
	{
		NET_LOG(LogTemp, Warning, TEXT("%hs Called, PS is nullptr"), __FUNCTION__);
		return;
	}
	bool bIsBlueTeam = PS->bIsBlueTeam;
	for (const auto& PlayerInfo : MatchPlayers)
	{
		if (bIsBlueTeam == PlayerInfo.bIsBlueTeam)
		{
			PlayerInfo.Controller->ClientRPC_OnAgentSelected(PS->DisplayName, SelectedAgentID);
		}
	}
}

bool AMatchGameMode::ReadyToStartMatch_Implementation()
{
	return LoggedInPlayerNum >= RequiredPlayerCount;
}

void AMatchGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	NET_LOG(LogTemp, Warning, TEXT("%hs Called"), __FUNCTION__);
	ValorantGameInstance->OnMatchHasStarted();
	
	TSubclassOf<AActor> ActorClass = APlayerStart::StaticClass();
	TArray<AActor*> OutActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ActorClass, OutActors);
	for (auto* Actor : OutActors)
	{
		auto* PlayerStart = Cast<APlayerStart>(Actor);
		if (PlayerStart->PlayerStartTag == FName("Attackers"))
		{
			AttackersStartPoint = PlayerStart;
		}
		else if (PlayerStart->PlayerStartTag == FName("Defenders"))
		{
			DefendersStartPoint = PlayerStart;
		}
	}

	StartSelectAgent();
}

bool AMatchGameMode::ReadyToEndMatch_Implementation()
{
	return bReadyToEndMatch;
}

void AMatchGameMode::LeavingMatch()
{
	bReadyToEndMatch = true;
	
	const bool bBlueWin = RequiredScore <= TeamBlueScore;
	const double TotalPlaySeconds = GetWorld()->GetRealTimeSeconds();
	
	for (auto& Pair : PlayerMatchInfoMap)
	{
		auto* Controller = Pair.Key;
		const auto* PlayerState = Controller->GetPlayerState<AAgentPlayerState>();
		CurrentMatchInfo->blue_score = TeamBlueScore;
		CurrentMatchInfo->red_score = TeamRedScore;
		CurrentMatchInfo->map_id = 0;

		FPlayerMatchDTO& PlayerMatchInfo = Pair.Value;
		PlayerMatchInfo.agent_id = PlayerState ? PlayerState->GetAgentID() : 0;
		PlayerMatchInfo.match_id = CurrentMatchInfo->match_id;
		
		FPlayerDTO& PlayerDto = PlayerMap[Controller];
		PlayerDto.total_playseconds += TotalPlaySeconds;
		if (bool bWin = !((PlayerMatchInfo.team == 0) ^ bBlueWin))
		{
			++PlayerDto.win_count;
		}
		else
		{
			++PlayerDto.defeat_count;
		}
		
		UDatabaseManager::GetInstance()->PutPlayer(PlayerDto);
		UDatabaseManager::GetInstance()->PutMatch(*CurrentMatchInfo);
		UDatabaseManager::GetInstance()->PostPlayerMatch(PlayerMatchInfo);
		
	}

	TArray<FPlayerMatchDTO> PlayerMatchInfoArray;
	PlayerMatchInfoMap.GenerateValueArray(PlayerMatchInfoArray);
	for (const auto& Pair : PlayerMatchInfoMap)
	{
		Pair.Key->ClientRPC_SaveMatchResult(*CurrentMatchInfo, PlayerMatchInfoArray);
	}
	
	PrintAllPlayerLogs();
	GetWorld()->GetFirstPlayerController()->ClientTravel("/Game/Maps/MainMap.MainMap", TRAVEL_Absolute);
}

void AMatchGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
	// 일정 시간 이후 매치 완전 종료
	// GetWorld()->GetTimerManager().SetTimer(RoundTimerHandle, this, &AMatchGameMode::LeavingMatch, LeavingMatchTime);
}

void AMatchGameMode::StartSelectAgent()
{
	SetRoundSubState(ERoundSubState::RSS_SelectAgent);
}

void AMatchGameMode::StartPreRound()
{
	++CurrentRound;
	SetRoundSubState(ERoundSubState::RSS_PreRound);
}

void AMatchGameMode::StartBuyPhase()
{
	++CurrentRound;
	SetRoundSubState(ERoundSubState::RSS_BuyPhase);
}

void AMatchGameMode::StartInRound()
{
	SetRoundSubState(ERoundSubState::RSS_InRound);
}

void AMatchGameMode::StartEndPhaseByTimeout()
{
	bool bBlueWin = IsShifted(); // Blue가 선공이니까 false라면 공격->진다, true라면 수비->이긴다
	HandleRoundEnd(bBlueWin, ERoundEndReason::ERER_Timeout);
	SetRoundSubState(ERoundSubState::RSS_EndPhase);
}

void AMatchGameMode::StartEndPhaseByEliminated(const bool bBlueWin)
{
	HandleRoundEnd(bBlueWin, ERoundEndReason::ERER_Eliminated);
	SetRoundSubState(ERoundSubState::RSS_EndPhase);
}

void AMatchGameMode::StartEndPhaseBySpikeActive()
{
	bool bBlueWin = !IsShifted(); // Blue가 선공이니까 false라면 공격->이긴다, true라면 수비->진다
	Spike->ServerRPC_Detonate();
	HandleRoundEnd(bBlueWin, ERoundEndReason::ERER_SpikeActive);
	SetRoundSubState(ERoundSubState::RSS_EndPhase);
}

void AMatchGameMode::StartEndPhaseBySpikeDefuse()
{
	bool bBlueWin = IsShifted(); // Blue가 선공이니까 false라면 공격->해체당했으니 진다, true라면 수비->해체했으니 이긴다
	HandleRoundEnd(bBlueWin, ERoundEndReason::ERER_SpikeDefuse);
	SetRoundSubState(ERoundSubState::RSS_EndPhase);
}

void AMatchGameMode::HandleRoundSubState_SelectAgent()
{
	for (const FMatchPlayer& MatchPlayer : MatchPlayers)
	{
		NotifyGameStart(MatchPlayer.Controller, false);
		
		if (MatchPlayer.bIsBlueTeam)
		{
			MatchPlayer.Controller->ClientRPC_ShowSelectUI(BlueTeamPlayerNameArray);
		}
		else
		{
			MatchPlayer.Controller->ClientRPC_ShowSelectUI(RedTeamPlayerNameArray);
		}
		MatchPlayer.Controller->ClientRPC_SetViewTargetOnAgentSelectCamera();
	}
	// 일정 시간 후에 요원 강제 선택 후 라운드 준비
	MaxTime = SelectAgentTime;
	GetWorld()->GetTimerManager().ClearTimer(RoundTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(RoundTimerHandle, this, &AMatchGameMode::StartPreRound, SelectAgentTime);
}

void AMatchGameMode::HandleRoundSubState_PreRound()
{
	if (CurrentRound == ShiftRound)
	{
		auto* MatchGameState = GetGameState<AMatchGameState>();
		if (MatchGameState)
		{
			MatchGameState->MulticastRPC_OnShift();
		}
	}
	ClearObjects();
	RespawnAll();
	TeamBlueRemainingAgentNum = TeamRedRemainingAgentNum = MatchPlayers.Num();

	// 스파이크 관련 상태 초기화
	bSpikePlanted = false;

	// 배리어 활성화를 위한 브로드캐스트
	OnStartPreRound.Broadcast();

	// 일정 시간 후에 라운드 시작
	MaxTime = PreRoundTime;
	GetWorld()->GetTimerManager().ClearTimer(RoundTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(RoundTimerHandle, this, &AMatchGameMode::StartInRound, PreRoundTime);
}

void AMatchGameMode::HandleRoundSubState_BuyPhase()
{
	ClearObjects();
	RespawnAll();
	TeamBlueRemainingAgentNum = TeamRedRemainingAgentNum = MatchPlayers.Num();

	// 스파이크 관련 상태 초기화
	bSpikePlanted = false;

	// 배리어 활성화를 위한 브로드캐스트
	OnStartPreRound.Broadcast();

	// 일정 시간 후에 라운드 시작
	MaxTime = BuyPhaseTime;
	GetWorld()->GetTimerManager().ClearTimer(RoundTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(RoundTimerHandle, this, &AMatchGameMode::StartInRound, BuyPhaseTime);
}

void AMatchGameMode::HandleRoundSubState_InRound()
{
	// 구매 페이즈가 끝나고 인 라운드로 전환될 때 모든 무기를 사용됨으로 표시
	MarkAllWeaponsAsUsed();

	// 열려있는 모든 상점 UI 강제로 닫기
	AMatchGameState* MatchGameState = GetGameState<AMatchGameState>();
	if (MatchGameState)
	{
		MatchGameState->MulticastRPC_CloseAllShops();
	}

	// 배리어 해제 및 로깅 시작을 위한 브로드캐스트
	OnStartInRound.Broadcast();

	// 일정 시간 후에 라운드 종료
	MaxTime = InRoundTime;
	GetWorld()->GetTimerManager().ClearTimer(RoundTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(RoundTimerHandle, this, &AMatchGameMode::StartEndPhaseByTimeout,
	                                       InRoundTime);
}

void AMatchGameMode::HandleRoundSubState_EndPhase()
{
	// 로깅 종료를 위한 브로드캐스트
	OnEndRound.Broadcast();
	
	// TODO: 라운드 상황에 따라 BuyPhase로 전환할 것인지 InRound로 전환할 것인지 아예 매치가 끝난 상태로 전환할 것인지 판단
	// 공수교대(->InRound) 조건: 3라운드가 끝나고 4라운드 시작되는 시점
	// 매치 종료 조건: 4승을 먼저 달성한 팀이 있는 경우 (6전 4선승제, 만약 3:3일 경우 단판 승부전)
	NET_LOG(LogTemp, Warning, TEXT("TeamBlueScore: %d, TeamRedScore: %d"), TeamBlueScore, TeamRedScore);
	if (RequiredScore <= TeamBlueScore || RequiredScore <= TeamRedScore)
	{
		NET_LOG(LogTemp, Warning, TEXT("ReadyToEndMatch"));
		const bool bBlueWin = RequiredScore <= TeamBlueScore;
		
		// 일정 시간 후에 매치 세션 종료
		GetWorld()->GetTimerManager().SetTimer(RoundTimerHandle, this, &AMatchGameMode::LeavingMatch, LeavingMatchTime);
		AMatchGameState* MatchGameState = GetGameState<AMatchGameState>();
		if (MatchGameState)
		{
			MatchGameState->MulticastRPC_OnMatchEnd(bBlueWin);
		}

		UE_LOG(LogTemp,Error,TEXT("최종 결과"));
		PrintAllPlayerLogs();
		return;
	}

	PrintAllPlayerLogs();
	// 일정 시간 후에 라운드 재시작
	MaxTime = EndPhaseTime;
	GetWorld()->GetTimerManager().ClearTimer(RoundTimerHandle);
	if (CurrentRound == TotalRound)
	{
		//
	}
	else if (CurrentRound == ShiftRound - 1)
	{
		GetWorld()->GetTimerManager().SetTimer(RoundTimerHandle, this, &AMatchGameMode::StartPreRound, EndPhaseTime);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(RoundTimerHandle, this, &AMatchGameMode::StartBuyPhase, EndPhaseTime);
	}
	
	// 라운드 종료 시 크레딧 보상 지급
	AwardRoundEndCredits();
}

void AMatchGameMode::SetRoundSubState(ERoundSubState NewRoundSubState)
{
	if (RoundSubState != NewRoundSubState)
	{
		AMatchGameState* MatchGameState = GetGameState<AMatchGameState>();
		if (nullptr == MatchGameState)
		{
			NET_LOG(LogTemp, Warning, TEXT("%hs Called, MatchGameState is nullptr"), __FUNCTION__);
		}
		RoundSubState = NewRoundSubState;

		// 구매 페이즈가 아닌 다른 상태로 변경될 때 열려있는 상점 UI 모두 닫기
		if (MatchGameState && RoundSubState != ERoundSubState::RSS_BuyPhase || RoundSubState !=
			ERoundSubState::RSS_PreRound)
		{
			// 모든 클라이언트에게 상점 닫기 이벤트 전파
			MatchGameState->MulticastRPC_CloseAllShops();
		}

		if (RoundSubState == ERoundSubState::RSS_SelectAgent)
		{
			HandleRoundSubState_SelectAgent();
		}
		else if (RoundSubState == ERoundSubState::RSS_PreRound)
		{
			HandleRoundSubState_PreRound();
		}
		else if (RoundSubState == ERoundSubState::RSS_BuyPhase)
		{
			HandleRoundSubState_BuyPhase();
		}
		else if (RoundSubState == ERoundSubState::RSS_InRound)
		{
			HandleRoundSubState_InRound();
		}
		else if (RoundSubState == ERoundSubState::RSS_EndPhase)
		{
			HandleRoundSubState_EndPhase();
		}
		MatchGameState->SetRoundSubState(RoundSubState, MaxTime);

		RemainRoundStateTime = MaxTime;
		MatchGameState->SetRemainRoundStateTime(RemainRoundStateTime);
	}
}

AActor* AMatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// TODO: 블루팀이냐 레드팀이냐, 누가 공격인지 수비냐에 따라 스폰 위치 다르게
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AMatchGameMode::ClearObjects()
{
	TArray<AActor*> Interactors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseInteractor::StaticClass(), Interactors);
	for (auto* Actor : Interactors)
	{
		auto* Interactor = Cast<ABaseInteractor>(Actor);
		if (Interactor && Interactor->HasOwnerAgent() == false)
		{
			Interactor->Destroy();
		}
		else
		{
			if (auto* Weapon = Cast<ABaseWeapon>(Interactor))
			{
				Weapon->ServerOnly_ClearAmmo();
			}
		}
	}

	// 월드에 spawn된 spike 제거
	DestroySpikeInWorld();
}

void AMatchGameMode::RespawnAll()
{
	for (auto& MatchPlayer : MatchPlayers)
	{
		MatchPlayer.bIsDead = false;

		FTransform SpawnTransform = FTransform::Identity;
		if (IsAttacker(MatchPlayer.bIsBlueTeam))
		{
			SpawnTransform = AttackersStartPoint->GetTransform();
		}
		else
		{
			SpawnTransform = DefendersStartPoint->GetTransform();
		}

		auto* agentPS = MatchPlayer.Controller->GetPlayerState<AAgentPlayerState>();
		if (nullptr == agentPS)
		{
			NET_LOG(LogTemp, Error, TEXT("%hs Called, agentPS is nullptr"), __FUNCTION__);
			return;
		}

		ResetAgentGAS(agentPS);
		RespawnPlayer(agentPS, MatchPlayer.Controller, SpawnTransform);
	}
	// 3초 후에 공격팀에게 스파이크 스폰
	FTimerHandle SpawnSpikeTimerHandle;
	GetWorldTimerManager().SetTimer(SpawnSpikeTimerHandle, this, &AMatchGameMode::SpawnSpikeForAttackers, 3.0f, false);
	// TODO: 팀 & 공수교대 여부에 따라 처리
}

void AMatchGameMode::RespawnPlayer(AAgentPlayerState* ps, AAgentPlayerController* pc, FTransform spawnTransform)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABaseAgent* Agent = Cast<ABaseAgent>(ps->GetPawn());
	
	if (Agent != nullptr && !Agent->IsDead())
	{
		Agent->SetActorTransform(spawnTransform);
		Agent->SetCanMove(true);
		return;
	}

	FAgentData* agentData = Cast<UValorantGameInstance>(GetGameInstance())->GetAgentData(ps->GetAgentID());
	Agent = GetWorld()->SpawnActor<ABaseAgent>(agentData->AgentAsset, spawnTransform);
	Agent->SetAgentID(ps->GetAgentID());
	ps->SetIsSpectator(false);
	ps->SetIsOnlyASpectator(false);

	APawn* oldPawn = pc->GetPawn();

	pc->Possess(Agent);

	if (oldPawn)
	{
		oldPawn->Destroy();
	}

	// ToDo 삭제!!!!
	Agent->DevCameraMode(true);
}

// 체력 정상화, 어빌리티 상태 초기화
void AMatchGameMode::ResetAgentGAS(AAgentPlayerState* AgentPS)
{
	AgentPS->GetBaseAttributeSet()->ResetAttributeData();
	AgentPS->GetAbilitySystemComponent()->ForceCleanupAllAbilities();
}

void AMatchGameMode::OnKill(AAgentPlayerController* Killer, AAgentPlayerController* Victim)
{
	// 킬러/희생자 정보 로깅
	FString KillerName = Killer ? Killer->GetPlayerState<APlayerState>()->GetPlayerName() : TEXT("없음");
	FString VictimName = Victim ? Victim->GetPlayerState<APlayerState>()->GetPlayerName() : TEXT("없음");
	NET_LOG(LogTemp, Warning, TEXT("OnKill 호출: 킬러=%s, 희생자=%s"), *KillerName, *VictimName);

	// 킬러에게 크레딧 보상
	if (Killer)
	{
		AAgentPlayerState* KillerPS = Killer->GetPlayerState<AAgentPlayerState>();
		if (KillerPS)
		{
			// TODO: 헤드샷 여부 확인 로직 추가
			bool bIsHeadshot = false; // 임시로 false로 설정

			UCreditComponent* CreditComp = KillerPS->FindComponentByClass<UCreditComponent>();
			if (CreditComp)
			{
				CreditComp->AwardKillCredits();

				NET_LOG(LogTemp, Warning, TEXT("크레딧 보상 지급: %s가 킬 보상을 받았습니다."), *KillerName);
			}

			KillerPS->OnKill();
		}
	}

	// 팀원들에게 어시스트 로깅 - 나중에 실제 어시스트 로직으로 교체
	if (Killer && Victim)
	{
		AAgentPlayerState* KillerPS = Killer->GetPlayerState<AAgentPlayerState>();
		AAgentPlayerState* VictimPS = Victim->GetPlayerState<AAgentPlayerState>();

		if (KillerPS && VictimPS)
		{
			bool bKillerIsBlue = KillerPS->bIsBlueTeam;

			// 같은 팀 플레이어에게 어시스트 데이터 추가 (실제로는 어시스트 여부 체크 필요)
			for (const FMatchPlayer& Player : MatchPlayers)
			{
				if (Player.Controller && Player.Controller != Killer && Player.bIsBlueTeam == bKillerIsBlue)
				{
					AAgentPlayerState* PS = Player.Controller->GetPlayerState<AAgentPlayerState>();
					if (PS)
					{
						// TODO: 실제 어시스트 여부 확인 필요
					}
				}
			}
		}
	}
	
	if (PlayerMatchInfoMap.Contains(Killer) && PlayerMatchInfoMap.Contains(Victim))
	{
		const bool bFirstKill = MatchPlayers.Num() - (TeamBlueRemainingAgentNum + TeamRedRemainingAgentNum) == 1;
		auto& KillerData = PlayerMatchInfoMap[Killer];
		KillerData.kill_count++;
		KillerData.first_kill_count += bFirstKill ? 1 : 0;
		auto& VictimData = PlayerMatchInfoMap[Victim];
		VictimData.death_count++;
		VictimData.first_death_count += bFirstKill ? 1 : 0;
	}
}

void AMatchGameMode::OnDie(AMatchPlayerController* Victim)
{
	if (!IsValid(this))
	{
		return;
	}
	int Blue = 0;
	int Red = 0;
	// 생존 플레이어 카운트 및 라운드 종료 처리
	for (auto& PlayerInfo : MatchPlayers)
	{
		if (PlayerInfo.Controller == Victim)
		{
			PlayerInfo.bIsDead = true;
		}

		if (PlayerInfo.bIsBlueTeam && false == PlayerInfo.bIsDead)
		{
			Blue++;
		}
		else if (!PlayerInfo.bIsBlueTeam && false == PlayerInfo.bIsDead)
		{
			Red++;
		}
	}
	TeamBlueRemainingAgentNum = Blue;
	TeamRedRemainingAgentNum = Red;
}

void AMatchGameMode::OnRevive(AMatchPlayerController* Reviver, AMatchPlayerController* Target)
{
	for (auto& PlayerInfo : MatchPlayers)
	{
		if (PlayerInfo.Controller == Cast<AAgentPlayerController>(Target))
		{
			PlayerInfo.bIsDead = false;
			break;
		}
	}
}

void AMatchGameMode::OnSpikePlanted(AAgentPlayerController* Planter)
{
	// 스파이크 설치자에게 크레딧 보상
	if (Planter)
	{
		AAgentPlayerState* PlanterPS = Planter->GetPlayerState<AAgentPlayerState>();
		if (PlanterPS)
		{
			UCreditComponent* CreditComp = PlanterPS->FindComponentByClass<UCreditComponent>();
			if (CreditComp)
			{
				CreditComp->AwardSpikeActionCredits(true);
			}
		}

		if (PlayerMatchInfoMap.Contains(Planter))
		{
			auto& PlanterData = PlayerMatchInfoMap[Planter];
			PlanterData.plant_count++;
		}
	}

	// 스파이크 설치 상태 업데이트
	bSpikePlanted = true;

	// 설치 완료 이벤트 알림 (모든 클라이언트에게)
	AMatchGameState* MatchGameState = GetGameState<AMatchGameState>();
	if (MatchGameState)
	{
		MatchGameState->MulticastRPC_OnSpikePlanted(Planter);
	}

	// 라운드 타이머 수정
	MaxTime = SpikeActiveTime;
	RemainRoundStateTime = MaxTime;
	GetWorld()->GetTimerManager().ClearTimer(RoundTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(RoundTimerHandle, this, &AMatchGameMode::StartEndPhaseBySpikeActive,
	                                       SpikeActiveTime);
}

void AMatchGameMode::OnSpikeDefused(AAgentPlayerController* Defuser)
{
	// 스파이크 해제자에게 크레딧 보상
	if (Defuser)
	{
		AAgentPlayerState* DefuserPS = Defuser->GetPlayerState<AAgentPlayerState>();
		if (DefuserPS)
		{
			UCreditComponent* CreditComp = DefuserPS->FindComponentByClass<UCreditComponent>();
			if (CreditComp)
			{
				CreditComp->AwardSpikeActionCredits(false);
			}
		}

		if (PlayerMatchInfoMap.Contains(Defuser))
		{
			auto& PlanterData = PlayerMatchInfoMap[Defuser];
			PlanterData.plant_count++;
		}
	}

	// 스파이크 설치 상태 업데이트
	bSpikePlanted = false;

	// 해제 완료 이벤트 알림 (모든 클라이언트에게)
	AMatchGameState* MatchGameState = GetGameState<AMatchGameState>();
	if (MatchGameState)
	{
		MatchGameState->MulticastRPC_OnSpikeDefused(Defuser);
	}

	StartEndPhaseBySpikeDefuse();
}

void AMatchGameMode::HandleRoundEnd(bool bBlueWin, const ERoundEndReason RoundEndReason)
{
	// 승패에 따라 팀 크레딧 지급
	for (const FMatchPlayer& Player : MatchPlayers)
	{
		if (Player.Controller)
		{
			// 플레이어 스테이트에서 크레딧 컴포넌트 찾기
			AAgentPlayerState* PS = Player.Controller->GetPlayerState<AAgentPlayerState>();
			if (PS)
			{
				UCreditComponent* CreditComp = PS->FindComponentByClass<UCreditComponent>();
				if (CreditComp)
				{
					// 팀 승패에 따라 크레딧 지급
					bool bIsWinner = (Player.bIsBlueTeam == bBlueWin);

					// 연속 패배 보너스 계산
					int32 ConsecutiveLosses = bIsWinner
						                          ? 0
						                          : (Player.bIsBlueTeam
							                             ? BlueTeamConsecutiveLosses
							                             : RedTeamConsecutiveLosses);

					CreditComp->AwardRoundEndCredits(bIsWinner, ConsecutiveLosses);
				}
			}
		}
	}

	// 연속 패배 카운터 업데이트
	if (bBlueWin)
	{
		BlueTeamConsecutiveLosses = 0;
		RedTeamConsecutiveLosses++;

		// 최대 3까지만 보너스 적용
		RedTeamConsecutiveLosses = FMath::Min(RedTeamConsecutiveLosses, 3);
	}
	else
	{
		RedTeamConsecutiveLosses = 0;
		BlueTeamConsecutiveLosses++;

		// 최대 3까지만 보너스 적용
		BlueTeamConsecutiveLosses = FMath::Min(BlueTeamConsecutiveLosses, 3);
	}

	if (bBlueWin)
	{
		++TeamBlueScore;
	}
	else
	{
		++TeamRedScore;
	}
	AMatchGameState* MatchGameState = GetGameState<AMatchGameState>();
	if (nullptr == MatchGameState)
	{
		NET_LOG(LogTemp, Warning, TEXT("%hs Called, MatchGameState is nullptr"), __FUNCTION__);
	}
	MatchGameState->SetTeamScore(TeamBlueScore, TeamRedScore);
	MatchGameState->MulticastRPC_HandleRoundEnd(bBlueWin, RoundEndReason);
}

// 크레딧 시스템 관련 함수 추가
void AMatchGameMode::AwardRoundEndCredits()
{
	// 현재 라운드 승패 정보 얻기 (임시로 팀 A가 이겼다고 가정)
	bool bTeamAWon = true; // 실제로는 라운드 결과에 따라 설정

	// 라운드 종료 후 모든 플레이어에게 크레딧 보상 지급
	for (const FMatchPlayer& Player : MatchPlayers)
	{
		if (Player.Controller)
		{
			// 플레이어 스테이트에서 크레딧 컴포넌트 찾기
			AAgentPlayerState* PS = Player.Controller->GetPlayerState<AAgentPlayerState>();
			if (PS)
			{
				UCreditComponent* CreditComp = PS->GetCreditComponent();
				if (CreditComp)
				{
					// 팀 승패에 따라 크레딧 지급
					bool bIsWinner = (Player.bIsBlueTeam == bTeamAWon);

					// 연속 패배 보너스 계산 (실제로는 팀별 연속 패배 횟수를 추적해야 함)
					int32 ConsecutiveLosses = 0;
					if (!bIsWinner)
					{
						// TODO: 팀별 연속 패배 횟수 추적 구현
						ConsecutiveLosses = 1; // 임시로 1로 설정
					}

					CreditComp->AwardRoundEndCredits(bIsWinner, ConsecutiveLosses);
				}
			}
		}
	}
}

// 모든 무기의 사용 상태를 true로 설정하는 함수 추가
void AMatchGameMode::MarkAllWeaponsAsUsed()
{
	// 모든 플레이어를 순회하면서 무기 사용 상태를 true로 설정
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AAgentPlayerController* PC = Cast<AAgentPlayerController>(It->Get());
		if (!PC)
			continue;

		ABaseAgent* Agent = PC->GetPawn<ABaseAgent>();
		if (!Agent)
			continue;

		// 플레이어가 가진 모든 무기를 사용된 상태로 표시
		ABaseWeapon* PrimaryWeapon = Agent->GetMainWeapon();
		if (PrimaryWeapon)
		{
			PrimaryWeapon->SetWasUsed(true);
		}

		ABaseWeapon* SecondWeapon = Agent->GetSubWeapon();
		if (SecondWeapon)
		{
			SecondWeapon->SetWasUsed(true);
		}
	}
}

ERoundSubState AMatchGameMode::GetRoundSubState() const
{
	return RoundSubState;
}

bool AMatchGameMode::CanOpenShop() const
{
	return RoundSubState == ERoundSubState::RSS_BuyPhase;
}

// 공격팀에게 스파이크 스폰
void AMatchGameMode::SpawnSpikeForAttackers()
{
	for (auto& MatchPlayer : MatchPlayers)
	{
		auto* AgentPS = MatchPlayer.Controller->GetPlayerState<AAgentPlayerState>();
		if (!AgentPS)
			continue;

		// 공격팀 플레이어 찾기
		if (AMatchGameMode::IsAttacker(AgentPS->bIsBlueTeam))
		{
			// 플레이어가 조종하는 에이전트 찾기
			ABaseAgent* Agent = Cast<ABaseAgent>(MatchPlayer.Controller->GetPawn());
			if (Agent)
			{
				// 스파이크 스폰
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				// 플레이어 발 아래에 스파이크 스폰
				FVector SpawnLocation = Agent->GetActorLocation() + Agent->GetActorForwardVector() * 100.0f;
				SpawnLocation.Z -= 80.0f; // 바닥에 가깝게

				Spike = GetWorld()->SpawnActor<ASpike>(SpikeClass, SpawnLocation,
				                                       FRotator::ZeroRotator, SpawnParams);
				// 공격팀 중 한 명에게만 스파이크 스폰

				break;
			}
		}
	}
}

void AMatchGameMode::SpawnDefaultWeapon()
{
	for (auto& MatchPlayer : MatchPlayers)
	{
		auto* agent = MatchPlayer.Controller->GetPawn<ABaseAgent>();
		if (nullptr == agent)
		{
			NET_LOG(LogTemp, Error, TEXT("%hs Called, agent is nullptr"), __FUNCTION__);
			return;
		}

		if (agent->GetMeleeWeapon() == nullptr) { 
			{
				if (MeleeAsset)
				{
					ABaseWeapon* knife = GetWorld()->SpawnActor<ABaseWeapon>(MeleeAsset);
					agent->ServerRPC_Interact(knife);
				}
			}
		}
		if (agent->GetSubWeapon() == nullptr) { 
			{
				if (ClassicAsset)
				{
					ABaseWeapon* gun = GetWorld()->SpawnActor<ABaseWeapon>(ClassicAsset);
					agent->ServerRPC_Interact(gun);
				}
			}
		}
	}
}

void AMatchGameMode::SpawnDefaultWeapon(ABaseAgent* agent)
{
	if (MeleeAsset == nullptr || ClassicAsset == nullptr)
	{
		return;
	}

	if (agent->GetMeleeWeapon() == nullptr)
	{
		ABaseWeapon* knife = GetWorld()->SpawnActor<ABaseWeapon>(MeleeAsset);
		agent->ServerRPC_Interact(knife);
	}
	
	if (agent->GetSubWeapon() == nullptr) 
	{
		ABaseWeapon* gun = GetWorld()->SpawnActor<ABaseWeapon>(ClassicAsset);
		agent->ServerRPC_Interact(gun);
	}
	
	else
	{
		agent->SwitchEquipment(EInteractorType::Melee);
	}
	
}

void AMatchGameMode::DestroySpikeInWorld()
{
	if (Spike)
	{
		Spike->Destroy();
	}
}

void AMatchGameMode::SubmitShotLog(AAgentPlayerController* pc, int32 fireCount, int32 hitCount,
	int32 headshotCount, int damage)
{
	if (PlayerMatchInfoMap.Contains(pc))
	{
		NET_LOG(LogTemp, Warning, TEXT("%hs Called, 로그 제출, Fire: %d / Hit: %d / Head: %d / Damage: %d"), __FUNCTION__, fireCount, hitCount, headshotCount, damage);
		auto& Data = PlayerMatchInfoMap[pc];
		Data.fire_count += fireCount;
		Data.hit_count += hitCount;
		Data.headshot_count += headshotCount;
		Data.total_damage += damage;
	}
}

void AMatchGameMode::PrintAllPlayerLogs() const
{
	// 맵이 비어있으면 간단히 메시지 찍고 리턴
	if (PlayerMatchInfoMap.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[GameMode] PlayerMatchInfoMap.Num() is 0"));
		return;
	}

	for (const TPair<AAgentPlayerController*, FPlayerMatchDTO>& Pair : PlayerMatchInfoMap)
	{
		const auto* Controller = Pair.Key;
		const auto& Data = Pair.Value;

		// // WinRound 배열을 "[1, 3, 4]" 형태로 변환
		// FString WinRounds;
		// {
		// 	for (int32 i = 0; i < LogData.WinRound.Num(); ++i)
		// 	{
		// 		WinRounds += FString::FromInt(LogData.WinRound[i]);
		// 		if (i < LogData.WinRound.Num() - 1)
		// 		{
		// 			WinRounds += TEXT(", ");
		// 		}
		// 	}
		// 	WinRounds = FString::Printf(TEXT("[%s]"), *WinRounds);
		// }
		//
		// // DefeatRound 배열을 "[2]" 형태로 변환
		// FString DefeatRounds;
		// {
		// 	for (int32 i = 0; i < LogData.DefeatRound.Num(); ++i)
		// 	{
		// 		DefeatRounds += FString::FromInt(LogData.DefeatRound[i]);
		// 		if (i < LogData.DefeatRound.Num() - 1)
		// 		{
		// 			DefeatRounds += TEXT(", ");
		// 		}
		// 	}
		// 	DefeatRounds = FString::Printf(TEXT("[%s]"), *DefeatRounds);
		// }

		// 구분선 출력 (헤더)
		UE_LOG(LogTemp, Log, TEXT("=== PlayerLog Start [%s] ==="), *Data.player_id);

		// 각 멤버를 직접 포맷팅해 출력
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Nickname=%s | Kill=%d | Death=%d | FireCount=%d | HitCount=%d | HeadshotCount=%d | TotalDamage=%d | PlantCount=%d | DefuseCount=%d | Team:%d"),
			*Data.player_id,
			Data.kill_count,
			Data.death_count,
			Data.fire_count,
			Data.hit_count,
			Data.headshot_count,
			Data.total_damage,
			Data.plant_count,
			Data.defuse_count,
			Data.team
		);

		// 구분선 출력 (푸터)
		UE_LOG(LogTemp, Log, TEXT("=== PlayerLog End   [%s] ===\n"), *Data.player_id);
	}
}

void AMatchGameMode::NotifyGameStart(AMatchPlayerController* PC, bool bDisplay)
{
	PC->ClientRPC_SetLoadingUI(bDisplay);
}


