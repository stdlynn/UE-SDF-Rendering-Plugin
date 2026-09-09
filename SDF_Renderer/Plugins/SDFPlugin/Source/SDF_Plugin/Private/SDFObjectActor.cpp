// Fill out your copyright notice in the Description page of Project Settings.

#include "SDFObjectActor.h"

// Plugin
#include "SDFComponent.h"

// Sets default values
ASDFObjectActor::ASDFObjectActor()
	: sdf_object{ CreateDefaultSubobject<USDFComponent>( TEXT( "SDFObject" ) ) }
{
	sdf_object->SetupAttachment( RootComponent );

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ASDFObjectActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ASDFObjectActor::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );
}