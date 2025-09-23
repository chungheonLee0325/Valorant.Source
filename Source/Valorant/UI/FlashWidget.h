﻿#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResourceManager/ValorantGameType.h"
#include "FlashWidget.generated.h"

class UImage;
class UCanvasPanel;
class UCanvasPanelSlot;

UCLASS()
class VALORANT_API UFlashWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// 섬광 강도 업데이트 - 위치 정보 추가
	UFUNCTION(BlueprintCallable, Category = "Flash")
	void UpdateFlashIntensity(float Intensity, FVector FlashWorldLocation = FVector::ZeroVector);

	// 섬광 효과 시작
	UFUNCTION(BlueprintCallable, Category = "Flash")
	void StartFlashEffect(float Duration, EFlashType FlashType = EFlashType::Default);

	// 섬광 효과 중지
	UFUNCTION(BlueprintCallable, Category = "Flash")
	void StopFlashEffect();

	// 섬광 타입 설정
	UFUNCTION(BlueprintCallable, Category = "Flash")
	void SetFlashType(EFlashType InFlashType);

protected:
	// 메인 캔버스 패널
	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* MainCanvas;

	// 전체 화면 오버레이 (하얀색/색상 틴트)
	UPROPERTY(meta = (BindWidget))
	UImage* FlashOverlay;

	// 방사형 빛 효과 이미지
	UPROPERTY(meta = (BindWidget))
	UImage* RadialFlashImage;

	// 섬광 타입별 색상 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flash Colors")
	FLinearColor DefaultFlashColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flash Colors")
	FLinearColor PhoenixFlashColor = FLinearColor(1.0f, 0.95f, 0.9f, 1.0f); // 약간 따뜻한 흰색

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flash Colors")
	FLinearColor KayOFlashColor = FLinearColor(0.95f, 0.98f, 1.0f, 1.0f); // 약간 차가운 흰색

	// 섬광 타입별 텍스처 (옵션)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flash Textures")
	UTexture2D* DefaultRadialTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flash Textures")
	UTexture2D* PhoenixRadialTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flash Textures")
	UTexture2D* KayORadialTexture;

	// 방사형 이미지 크기 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flash Settings", meta = (AllowPrivateAccess = "true"))
	float RadialImageSizeMultiplier = 2.5f;  // 화면 크기의 배수


private:
	// 현재 섬광 강도
	float CurrentFlashIntensity = 0.0f;

	// 현재 섬광 타입
	EFlashType CurrentFlashType;

	// 현재 섬광 색상
	FLinearColor CurrentFlashColor = FLinearColor::White;

	// 섬광 월드 위치 저장
	FVector StoredFlashWorldLocation;
	FVector2D FixedFlashScreenPosition;
	bool bHasStoredWorldLocation = false;

	// 섬광 위치를 화면 좌표로 변환
	FVector2D ConvertWorldToScreenPosition(FVector WorldLocation);

	// 색상 가져오기
	FLinearColor GetColorForFlashType(EFlashType FlashType);

	// 텍스처 가져오기
	UTexture2D* GetTextureForFlashType(EFlashType FlashType);

	// 방사형 이미지 위치 업데이트
	void UpdateRadialImagePosition(FVector2D ScreenPosition);

	// 페이드 효과 계산
	float CalculateFadeIntensity(float BaseIntensity);
};