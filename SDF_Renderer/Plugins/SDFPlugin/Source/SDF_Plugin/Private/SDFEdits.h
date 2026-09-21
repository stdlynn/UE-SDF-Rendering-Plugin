#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SDFEdits.generated.h"

UENUM(BlueprintType)
enum class EOperation : uint8
{
	Add       UMETA( DisplayName = "Add" ),
	Subtract  UMETA( DisplayName = "Subtract" ),
	Intserect UMETA( DisplayName = "Intersect" )
};

// SDF edit representation used for the SDF component
USTRUCT( BlueprintType )
struct FGameSDF
{
	GENERATED_BODY()

	UPROPERTY( EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0") )
	float Radius{ 100.0f };

	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	EOperation Operation{ EOperation::Add };

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bDoesSmoothing{ false };

	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	float Smoothing{ 0.0f };
};

// SDF edit representation sent to the render thread
struct FRenderSDF
{
	FRenderSDF( const FGameSDF& GameSDF, FVector3f Pos )
		: Pos{ Pos }
		, Radius{ GameSDF.Radius }
		, Operation{ GameSDF.Operation }
		, bSmoothed{ GameSDF.bDoesSmoothing }
		, Smoothing{ GameSDF.Smoothing } {}

	FVector3f Pos;
	float Radius;
	EOperation Operation;
	bool bSmoothed;
	float Smoothing;
};

// GPU SDF edits
struct alignas(16) FGPUSDF
{
	// Convert to GPU unit scale
	FGPUSDF( const FRenderSDF& RenderSDF, uint32 ID, float GPUUnitScale );

	FVector3f Pos;
	float Radius;
	uint32 Flags; // bits 0-1: operation, 2: bSmoothed
	float Smoothing;
	uint32 ID;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class USDFComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	USDFComponent();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
#if WITH_EDITOR
	virtual void PostEditComponentMove( bool bFinished ) override;
#endif

	const FGameSDF& GetSDF() const { return GameSDF; }
	uint64 GetID() const { return ID; }

	UFUNCTION( BlueprintCallable )
	void SetRadius( float NewRadius )
	{
		GameSDF.Radius = NewRadius;
		SetDirty();
	}

	UFUNCTION( BlueprintCallable )
	void SetOperation( EOperation NewOperation )
	{
		GameSDF.Operation = NewOperation;
		SetDirty();
	}

	UFUNCTION(BlueprintCallable)
	void SetDoesSmoothing(bool bNewDoesSmoothing)
	{
		GameSDF.bDoesSmoothing = bNewDoesSmoothing;
		SetDirty();
	}


	UFUNCTION( BlueprintCallable )
	void SetSmoothing( float NewSmoothing )
	{
		GameSDF.Smoothing = NewSmoothing;
		SetDirty();
	}

protected:

	virtual void OnUpdateTransform( EUpdateTransformFlags TransformFlags,
									ETeleportType TeleportType = ETeleportType::None ) override;

private:

	inline static uint64 NextID{ 0 };

	void SetDirty() const;

	// Game params, game thread
	UPROPERTY( EditAnywhere,
		BlueprintReadOnly, // Edit through setters
		Category = "SDF",
		meta = (DisplayName = "Shape",
			ShowOnlyInnerProperties,
			AllowPrivateAccess = "true") )
	FGameSDF GameSDF{};
	uint64 ID;

}; // USDFComponent

UCLASS()
class ASDFActor : public AActor
{
	GENERATED_BODY()

public:

	ASDFActor();

private:

	UPROPERTY( EditAnywhere,
		BlueprintReadOnly,
		Category = "Components",
		meta = (AllowPrivateAccess = "true") )
	USDFComponent* SDFComp;

}; // ASDFActor

UCLASS()
class USDFSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	void RegisterSDF( const USDFComponent& Comp );
	void UnregisterSDF( const USDFComponent& Comp );
	void SetSDFDirty( const USDFComponent& Comp );

	const auto& GetRenderSDFs() const { return RenderSDFs; }
	bool GetAndResetDirty()
	{
		const bool bWasDirty{ bDirty };
		bDirty = false;
		return bWasDirty;
	}

protected:

	virtual bool DoesSupportWorldType( const EWorldType::Type WorldType ) const override;

private:

	// Render params, game thread
	TMap<uint64, FRenderSDF> RenderSDFs;
	bool bDirty{ true };

}; // USDFSubsystem