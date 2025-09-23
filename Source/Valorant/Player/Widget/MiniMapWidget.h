// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MiniMapWidget.generated.h"

enum class EVisibilityState : uint8;
class AAgentPlayerState;
class ABaseAgent;
class UCanvasPanel;

/**
 * 
 */
UCLASS(meta = (DisableNativeTick))
class VALORANT_API UMiniMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override; // 위젯이 생성될 때 호출되는 함수, 부모 클래스 함수 오버라이드

	// 미니맵에 에이전트 추가 함수
    UFUNCTION(BlueprintCallable, Category = "Minimap") 
    void AddPlayerToMinimap(const AAgentPlayerState* Player);
    
    // 미니맵에서 에이전트 제거 함수
    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void RemovePlayerFromMinimap(AAgentPlayerState* Player); 
    
	// 미니맵의 스케일을 설정하는 함수 (월드 단위 -> 미니맵 픽셀 변환 비율)
    UFUNCTION(BlueprintCallable, Category = "Minimap") 
    void SetMinimapScale(float NewScale); 
    
    // 미니맵 업데이트 - 월드 좌표를 미니맵 좌표로 변환하는 함수 (월드 좌표 -> 미니맵 좌표 변환)
    UFUNCTION(BlueprintCallable, Category = "Minimap") 
    FVector2D WorldToMinimapPosition(const FVector& TargetActorLocation); 
	// 주기적으로 모든 에이전트 검색하여 등록하는 함수 추가
	UFUNCTION(BlueprintCallable, Category = "Minimap")
	void ScanPlayer();
	
protected:
    UPROPERTY() 
    TArray<const AAgentPlayerState*> PlayerArray; // 미니맵에 표시될 모든 플레이어
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	TMap<AAgentPlayerState*, UUserWidget*> AgentIconMap; // 플레이어와 아이콘 위젯 매핑
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap") 
    AAgentPlayerState* MyPlayerState = nullptr; // 현재 미니맵을 보고 있는 플레이어의 에이전트
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "Minimap") 
	class UCanvasPanel* CanvasPanel;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "Minimap") 
    class UImage* MinimapBackground; // 미니맵 배경 이미지 위젯
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "Minimap")
	class UCanvasPanel* IconContainer; // 아이콘 컨테이너
	
    // 미니맵 스케일 - 월드 좌표를 미니맵 좌표로 변환하는 비율 (월드 단위 -> 미니맵 픽셀)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    float MapScale = 0.1f; // 기본 맵 스케일 설정 (1 월드 단위 = 0.1 미니맵 픽셀)
    
    // 미니맵 사이즈 (픽셀)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    float MinimapSize = 450.0f; // 기본 미니맵 사이즈 설정 (450x450 픽셀)

	FVector2D ConvertOffset = FVector2D(18, -14);
	
    // 미니맵 중앙점 -미니맵의 중앙 좌표 (픽셀)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    FVector2D MinimapCenter = FVector2D(MinimapSize / 2, MinimapSize / 2); // 미니맵 중앙점 계산 (미니맵 크기의 절반)
    
    // 모든 에이전트의 미니맵 아이콘 위치와 상태를 업데이트하는 함수
    UFUNCTION(BlueprintCallable, Category = "Minimap") 
    void UpdateAgentIcons(); 
    
    // 미니맵에 아이콘 생성 함수 (블루프린트에서 구현)
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Minimap") 
    void CreateAgentIcon(const AAgentPlayerState* Player, const UTexture2D* IconTexture, const EVisibilityState VisState, const int Flag); 
    
    // 미니맵 아이콘 업데이트 함수 (블루프린트에서 구현)
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Minimap") 
    void UpdateAgentIcon(const AAgentPlayerState* Player, const FVector2D& Position, const UTexture2D* IconTexture, const EVisibilityState VisState, const int Flag);
    
	// 아이콘 크기 변수 추가
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	FVector2D IconSize = FVector2D(12.0f, 12.0f);
    
	// 스캔 주기 타이머 추가
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	float ScanInterval = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	float TimeSinceLastScan = 0.0f;

private:
	FTimerHandle ScanTimerHandle;
	void Scan();
};
