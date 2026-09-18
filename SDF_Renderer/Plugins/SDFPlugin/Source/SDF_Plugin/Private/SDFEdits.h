#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SDFEdits.generated.h"

// SDF edit representation used for the SDF component
USTRUCT( BlueprintType )
struct FGameSDF
{
	GENERATED_BODY()

	UPROPERTY( EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0") )
	float Radius{ 100.0f };
};

// SDF edit representation sent to the render thread
struct FRenderSDF
{
	FRenderSDF( const FGameSDF& GameSDF, FVector3f Pos )
		: Pos{ Pos }
		, Radius{ GameSDF.Radius }
	{}

	FVector3f Pos;
	float Radius;
};

// GPU SDF edits
struct alignas(16) FGPUSDF
{
	// Convert to GPU unit scale
	FGPUSDF( const FRenderSDF& RenderSDF, uint32 ID, float GPUUnitScale )
		: Pos{ RenderSDF.Pos * GPUUnitScale }
		, Radius{ RenderSDF.Radius * GPUUnitScale }
		, ID{ ID }
	{}

	FVector3f Pos;
	float Radius;
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
	virtual void PostEditComponentMove(bool bFinished) override;
#endif

	const FGameSDF& GetSDF() const { return GameSDF; }
	uint64 GetID() const { return ID; }

	UFUNCTION( BlueprintCallable )
	void SetRadius( float NewRadius )
	{
		GameSDF.Radius = NewRadius;
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