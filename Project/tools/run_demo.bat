@echo off
rem Runs the Release build in demo mode (loops the game over sequence).
rem Build the Release configuration in Visual Studio first.
set EXE=%~dp0..\..\generated\outputs\Release\MyGameEngine.exe
if not exist "%EXE%" (
  echo Release build not found: %EXE%
  pause
  exit /b 1
)
start "" "%EXE%" --demo
