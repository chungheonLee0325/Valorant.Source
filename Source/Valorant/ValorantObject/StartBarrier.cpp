﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "StartBarrier.h"

#include "GameManager/MatchGameMode.h"
#include "Kismet/GameplayStatics.h"


AStartBarrier::AStartBarrier()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bNetLoadOnClient = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierMesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
}

void AStartBarrier::BeginPlay()
{
	Super::BeginPlay();
	AMatchGameMode* GameMode = Cast<AMatchGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode)
	{
		GameMode->OnStartPreRound.AddDynamic(this,&AStartBarrier::ActiveBarrier);
		GameMode->OnStartInRound.AddDynamic(this,&AStartBarrier::DeactiveBarrier);
	}
}

void AStartBarrier::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AStartBarrier::DeactiveBarrier()
{
	Multicast_DeactiveBarrier();
}

void AStartBarrier::ActiveBarrier()
{
	Multicast_ActiveBarrier();
}

void AStartBarrier::Multicast_ActiveBarrier_Implementation()
{
	if (Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetVisibility(true);
	}
}

void AStartBarrier::Multicast_DeactiveBarrier_Implementation()
{
	if (Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(false);
	}
}
