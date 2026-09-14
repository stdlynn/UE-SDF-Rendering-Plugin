@echo off
setlocal

set UAT="C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat"
set PROJECT=%~dp0SDF_Renderer.uproject
set ARCHIVE=%~dp0Builds\Dev

call %UAT% BuildCookRun ^
  -project="%PROJECT%" ^
  -noP4 -platform=Win64 -clientconfig=Development ^
  -build -cook -stage -pak -archive ^
  -archivedirectory="%ARCHIVE%" ^
  -iterativecooking

endlocal