#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SDFResourceSubsystem.generated.h"

class FSDFSceneViewExtension;

UCLASS()
class SDFPLUGIN_API USDFResourceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	TSharedPtr<FSDFSceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;
};