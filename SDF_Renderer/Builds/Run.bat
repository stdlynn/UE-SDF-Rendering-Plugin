"./Dev/Windows/SDF_Renderer.exe" -windowed -ResX=2560 -ResY=1440 ^
  -ExecCmds="r.VSync 0, t.MaxFPS 0, r.ScreenPercentage 100, stat unit, setbind F9 ProfileGPU" ^
  -DPCVars="r.SDFPlugin.ProfileCount=9999"