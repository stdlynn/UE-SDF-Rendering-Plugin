// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto
// 
// Main Module to set Shader Directories

#include "SDFPlugin.h"

#define LOCTEXT_NAMESPACE "SDFPlugin"

void FSDFPlugin::StartupModule()
{
	// Set up the Shader Directories
	const FString BaseDir = IPluginManager::Get().FindPlugin( TEXT( "SDFPlugin" ) )->GetBaseDir();
	const FString PluginShaderDir = FPaths::Combine( BaseDir, TEXT( "Shaders" ) );
	AddShaderSourceDirectoryMapping( TEXT( "/Plugins/SDFPlugin" ), PluginShaderDir );
}

void FSDFPlugin::ShutdownModule() {}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE( FSDFPlugin, SDFPlugin );