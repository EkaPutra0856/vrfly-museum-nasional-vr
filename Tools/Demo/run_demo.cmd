@echo off
rem Usage: run_demo.cmd [script.json] [extra UE args]   (set MUSEUM_MAP=/Game/Museum/Maps/L_MuseumGallery for the 3D gallery)
rem Runs the autopilot tour of L_MuseumDemo (or MUSEUM_MAP) in a 1080p window at a fixed 30 fps game step and captures every frame to
rem Saved\DemoVideo\Frames (markers in Saved\DemoVideo\markers.txt). Then: python Tools\Demo\make_mp4.py
rem Editor must be CLOSED.
set ROOT=%~dp0..\..
set UPROJ=%ROOT%\Museum.uproject
if "%UE_ROOT%"=="" (echo Set UE_ROOT to the Unreal Engine 5.8 install folder& exit /b 1)
set UE=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe
if "%MUSEUM_MAP%"=="" set MUSEUM_MAP=/Game/Museum/Maps/L_MuseumDemo
set SCRIPT=%~1
if "%SCRIPT%"=="" set SCRIPT=%ROOT%\Content\MuseumVR\Demo\MuseumVRDemoScript.json
shift
if not exist "%ROOT%\Saved\DemoVideo" mkdir "%ROOT%\Saved\DemoVideo"
"%UE%" "%UPROJ%" %MUSEUM_MAP% -game -nohmd -MuseumVRDemo -MuseumVRDemoCapture "-MuseumVRDemoScript=%SCRIPT%" -UseFixedTimeStep -FPS=30 -NoVSync -NoScreenMessages -Windowed -ResX=1920 -ResY=1080 -log "-ABSLOG=%ROOT%\Saved\DemoVideo\demo_run.log" %1 %2 %3 %4 %5
