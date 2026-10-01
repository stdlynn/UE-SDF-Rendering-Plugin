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

	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	EOperation Operation{ EOperation::Add };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (
		EditCondition = "Operation == EOperation::Subtract", EditConditionHides))
	bool bColorsSubtraction{ false };

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EPrimitive Primitive{ EPrimitive::Sphere };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0",
		EditCondition = "Primitive == EPrimitive::Sphere || Primitive == EPrimitive::Line",
		EditConditionHides))
	float Radius{ 100.0f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0",
		EditCondition = "Primitive == EPrimitive::Box", EditConditionHides))
	float Rounding{ 0.0f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (
		DisplayName = "Line End Position",
		EditCondition = "Primitive == EPrimitive::Line", EditConditionHides))
	FVector3f LinePosB{ 0.0, 0.0, 100.0 };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0",
		DisplayName = "Extent (half)",
		EditCondition = "Primitive == EPrimitive::Box", EditConditionHides))
	FVector3f BoxSize{ 100.0, 100.0, 100.0 };

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bSmooths{ false };

	UPROPERTY( EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0") )
	float Smoothing{ 1.0f };

	UPROPERTY( EditAnywhere, BlueprintReadOnly, meta = 
		(ClampMin = "0",
		ClampMax = "255") )
	int MaterialIndex{ 0 };
};

// SDF edit representation sent to the render thread
struct FRenderSDF
{
	FRenderSDF( const FGameSDF& GameSDF, const FVector3f& Pos, const FQuat& Quat )
		: Pos{ Pos }
		, Primitive{ GameSDF.Primitive }
		, Quat{ Quat }
		, Operation{ GameSDF.Operation }
		, bColorsSubtraction{ GameSDF.bColorsSubtraction }
		, bSmooths{ GameSDF.bSmooths }
		, SmoothingStrength{ GameSDF.Smoothing }
		, MatIdx{ static_cast<uint32>(GameSDF.MaterialIndex) }
	{
		switch (GameSDF.Primitive)
		{
			case EPrimitive::Sphere: Data = FVector4f(GameSDF.Radius, 0.f, 0.f, 0.f); break;
			case EPrimitive::Line: Data = FVector4f(GameSDF.LinePosB, GameSDF.Radius); break;
			case EPrimitive::Box: Data = FVector4f(GameSDF.BoxSize, GameSDF.Rounding); break;
			default: Data = FVector4f(0.f, 0.f, 0.f, 0.f); break;
		}
	}

	FVector3f Pos;
	EPrimitive Primitive;
	FQuat Quat; // Data1 in FGPUSDF
	EOperation Operation;
	bool bColorsSubtraction;
	FVector4f Data; // Data0 in FGPUSDF
	bool bSmooths;
	float SmoothingStrength;
	uint32 MatIdx;
};

// GPU SDF edits
struct alignas(16) FGPUSDF
{
	// Convert to GPU unit scale
	FGPUSDF( const FRenderSDF& RenderSDF, uint32 ID, float GPUUnitScale );

	void SetPrimitive( const EPrimitive NewPrimitive );
	void SetSphereData( const float Radius );
	void SetLineData( const FVector3f& B, const float Radius );
	void SetBoxData( const FVector3f& HalfExtent, const FQuat& Quat, const float Rounding );
	void SetOperation( const EOperation NewOperation );
	void SetColorsSubtraction( const bool bEnabled );
	void EnableSmoothing( const bool bEnabled );
	[[nodiscard]] EPrimitive GetPrimitive() const { return static_cast<EPrimitive>( Flags & 0b11 ); }
	[[nodiscard]] float GetSphereRadius() const { return Data0.X; }
	[[nodiscard]] FVector3f GetLinePosB() const { return FVector3f( Data0.X, Data0.Y, Data0.Z ); }
	[[nodiscard]] float GetLineRadius() const { return Data0.W; }
	[[nodiscard]] FVector3f GetBoxHalfExtent() const { return FVector3f( Data0.X, Data0.Y, Data0.Z ); }
	[[nodiscard]] float GetBoxRounding() const { return Data0.W; }
	[[nodiscard]] FQuat GetBoxQuat() const { return FQuat(Data1.X, Data1.Y, Data1.Z, Data1.W); }
	[[nodiscard]] EOperation GetSDFOperation() const { return static_cast<EOperation>((Flags >> 2) & 0b1); }
	[[nodiscard]] bool GetColorsSubtraction() const { return Flags & 0b10000; }
	[[nodiscard]] bool GetSDFSmooths() const { return Flags & 0b1000; }

	FVector3f Pos;
	uint32 Flags; // bits 0-1: primitive, 2: operation, 3: bSmooths, 4: bColorsSubtraction
	FVector4f Data0; // Per-primitive data (e.g. radius)
	FVector4f Data1; // Per-primitive data
	float SmoothingStrength;
	uint32 ID;
	uint32 MatIdx;
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

	void SetRounding( float NewRounding )
	{
		GameSDF.Rounding = NewRounding;
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

	UFUNCTION( BlueprintCallable )
	void SetMaterialIndex( int NewMaterialIndex )
	{
		GameSDF.MaterialIndex = NewMaterialIndex;
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