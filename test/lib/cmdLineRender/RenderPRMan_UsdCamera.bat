@echo off
REM PRMan V2 batch render of usdCamera_PRMan_v2_rs.ma (PNG output).
REM Copy/adapt paths to match your machine.

setlocal

set BUILD_DIR=D:\test\build\RelWithDebInfo
set MAYA_BIN=D:\GIT\maya\builds\main\maya\build\RelWithDebInfo\runTime\bin
set SCENE_SRC=d:\GIT\maya-hydra-opensource\test\lib\cmdLineRender\scenes\basic\usdCamera_PRMan_v2_rs.ma
set OUT_ROOT=%BUILD_DIR%\test\Temporary\usdCamera_PRMan\projects\default\images
set OUT_FILE=%OUT_ROOT%\usdCamera.png

set HD_PRMAN_RENDER_SETTINGS_DRIVE_RENDER_PASS=1
set HD_PRMAN_DISABLE_ADAPTIVE_SAMPLING=1
set TF_DEBUG=HDPRMAN_* MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS
set TF_DEBUG_EMIT_CODE_NAMES_WITH_MESSAGES=1
set TF_DEBUG_OUTPUT_FILE=stderr

if not exist "%OUT_ROOT%" mkdir "%OUT_ROOT%"

echo Rendering to: %OUT_FILE%
echo.

"%MAYA_BIN%\mayabatch.exe" -command "Render -renderer mayaHydra -rfn \"HdPrmanLoaderRendererPlugin\" -x 960 -y 540 -cam \"|stage1|stageShape1,/camera1\" -rd \"%OUT_ROOT%\" -im usdCamera -of png \"%SCENE_SRC%\""

if exist "%OUT_FILE%" (
    echo SUCCESS: %OUT_FILE%
) else (
    echo FAILED: expected output not found: %OUT_FILE%
    dir "%OUT_ROOT%" 2>nul
    exit /b 1
)

endlocal
