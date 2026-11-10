@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
  echo Right-click this file and choose Run as administrator
  pause
  exit /b
)
xcopy /E /I /Y "%~dp0build\MetaTune_artefacts\Release\VST3\MetaTune.vst3" "C:\Program Files\Common Files\VST3\MetaTune.vst3"
echo.
echo Done. Now rescan plugins in FL Studio.
pause
