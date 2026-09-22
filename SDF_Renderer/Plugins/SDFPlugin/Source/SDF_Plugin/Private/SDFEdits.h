#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SDFEdits.generated.h"

UENUM( BlueprintType )
enum class EOperation : uint8
{
	Add      UMETA( DisplayName = "Add" ),
	Subtract UMETA( DisplayName = "Subtract" )
};

UENUM( BlueprintType )
enum class EPrimitive : uint8
{
	Sphere UMETA( DisplayName = "Sphere" ),
	Line   UMETA( DisplayName = "Line"),
	Box	   UMETA( DisplayName = "Box")
};

// SDF edit representation used for the SDF component
USTRUCT( BlueprintType )
struct FGameSDF
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EPrimitive Primitive{ EPrimitive::Sphere };

	UPROPERTY( EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0") )
	float Radius{ 100.0f };

	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	EOperation Operation{ EOperation::Add };

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bSmooths{ false };

	UPROPERTY( EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0") )
	float Smoothing{ 0.0f };
};

// SDF edit representation sent to the render thread
struct FRenderSDF
{
	FRenderSDF( const FGameSDF& GameSDF, FVector3f Pos )
		: Pos{ Pos }
		, Radius{ GameSDF.Radius }
		, Operation{ GameSDF.Operation }
		, bSmooths{ GameSDF.bSmooths }
		, SmoothingStrength{ GameSDF.Smoothing } {}

	FVector3f Pos;
	EPrimitive Primitive;
	float Radius;
	EOperation Operation;
	bool bSmooths;
	float SmoothingStrength;
};

// GPU SDF edits
struct alignas(16) FGPUSDF
{
	// Convert to GPU unit scale
	FGPUSDF( const FRenderSDF& RenderSDF, uint32 ID, float GPUUnitScale );

	void SetPrimitive( const EPrimitive NewPrimitive );
	void SetOperation( const EOperation NewOperation );
	void EnableSmoothing( const bool Enabled );

	FVector3f Pos;
	float Radius;
	uint32 Flags; // bit 0: operation, 1: bSmooths
	float SmoothingStrength;
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

	UFUNCTION(BlueprintCallable)
	void SetPrimitive( EPrimitive NewPrimitive )
	{
		GameSDF.Primitive = NewPrimitive;
		SetDirty();
	}

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
		GameSDF.bSmooths = bNewDoesSmoothing;
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