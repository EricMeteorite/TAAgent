@echo off
chcp 65001 >nul 2>&1
setlocal EnableDelayedExpansion

:: ============================================================
::  TAAgent 一键管理工具
::  双击运行，按编号选择操作即可
:: ============================================================

set "TAAGENT_ROOT=%~dp0"
:: Remove trailing backslash
if "%TAAGENT_ROOT:~-1%"=="\" set "TAAGENT_ROOT=%TAAGENT_ROOT:~0,-1%"

set "TOOLS=%TAAGENT_ROOT%\tools"
set "RUNTIME=%TAAGENT_ROOT%\.taagent-local"
set "CONFIG_FILE=%RUNTIME%\config\taagent_user.cfg"

:: Load saved config
set "SAVED_PROJECT_DIR="
set "SAVED_RENDERDOC_EXE="
if exist "%CONFIG_FILE%" (
    for /f "usebackq tokens=1,* delims==" %%a in ("%CONFIG_FILE%") do (
        if "%%a"=="PROJECT_DIR" set "SAVED_PROJECT_DIR=%%b"
        if "%%a"=="RENDERDOC_EXE" set "SAVED_RENDERDOC_EXE=%%b"
    )
)
if defined SAVED_RENDERDOC_EXE call :NORMALIZE_RENDERDOC_PATH SAVED_RENDERDOC_EXE

:MENU
cls
echo.
echo  ============================================================
echo              TAAgent 一键管理工具
echo  ============================================================
echo.
echo   -- 首次使用 ------------------------------------------------
echo.
echo    [1]  首次安装 / 初始化环境
echo.
echo   -- Unreal Engine -------------------------------------------
echo.
echo    [2]  部署 TAAgent 插件到我的 UE 项目
echo    [3]  启动 Unreal MCP 服务
echo    [4]  生成 UE Python Console 粘贴代码
echo    [5]  从我的 UE 项目卸载 TAAgent 插件
echo.
echo   -- RenderDoc -----------------------------------------------
echo.
echo    [6]  启动 RenderDoc (本地隔离模式)
echo    [7]  启动 RenderDoc MCP 服务
echo    [10]  触发当前 Live Capture 抓帧
echo.
echo   -- 设置 / 维护 --------------------------------------------
echo.
echo    [8]  设置我的 UE 项目路径
echo    [9]  设置 RenderDoc 路径
echo    [0]  完全卸载 TAAgent (清理一切)
echo.
echo   ------------------------------------------------------------
echo    [Q]  退出
echo.

if defined SAVED_PROJECT_DIR (
    echo   当前 UE 项目: %SAVED_PROJECT_DIR%
) else (
    echo   当前 UE 项目: [未设置]
)
if defined SAVED_RENDERDOC_EXE (
    echo   RenderDoc 路径: %SAVED_RENDERDOC_EXE%
) else (
    echo   RenderDoc 路径: [自动搜索]
)
echo.

set "CHOICE="
set /p "CHOICE=  请输入编号: "

if /i "%CHOICE%"=="1" goto DO_SETUP
if /i "%CHOICE%"=="2" goto DO_DEPLOY_PLUGIN
if /i "%CHOICE%"=="3" goto DO_START_UNREAL_MCP
if /i "%CHOICE%"=="4" goto DO_SHOW_SNIPPET
if /i "%CHOICE%"=="5" goto DO_REMOVE_PLUGIN
if /i "%CHOICE%"=="6" goto DO_OPEN_RENDERDOC
if /i "%CHOICE%"=="7" goto DO_START_RENDERDOC_MCP
if /i "%CHOICE%"=="10" goto DO_TRIGGER_RENDERDOC_CAPTURE
if /i "%CHOICE%"=="8" goto DO_SET_PROJECT
if /i "%CHOICE%"=="9" goto DO_SET_RENDERDOC
if /i "%CHOICE%"=="0" goto DO_FULL_UNINSTALL
if /i "%CHOICE%"=="Q" goto :EOF

echo.
echo  无效选项，请重新选择。
timeout /t 2 >nul
goto MENU

:: ============================================================
:: [1] 首次安装
:: ============================================================
:DO_SETUP
cls
echo.
echo  ============================================
echo   正在初始化 TAAgent 环境...
echo  ============================================
echo.
powershell -ExecutionPolicy Bypass -File "%TOOLS%\setup_local.ps1"
if errorlevel 1 (
    echo.
    echo  [错误] 安装失败，请检查上方输出。
) else (
    echo.
    echo  [完成] TAAgent 环境初始化成功！
)
echo.
pause
goto MENU

:: ============================================================
:: [2] 部署插件到 UE 项目
:: ============================================================
:DO_DEPLOY_PLUGIN
cls
echo.
if defined SAVED_PROJECT_DIR goto DEPLOY_HAS_PATH
echo  你还没有设置 UE 项目路径。
echo.
echo  请输入你的 UE 项目文件夹的完整路径
echo  (就是包含 .uproject 文件的那个文件夹)
echo.
set /p "SAVED_PROJECT_DIR=  项目路径: "
set "SAVED_PROJECT_DIR=%SAVED_PROJECT_DIR:"=%"
if not defined SAVED_PROJECT_DIR (
    echo  未输入路径，返回菜单。
    timeout /t 2 >nul
    goto MENU
)
call :SAVE_CONFIG
:DEPLOY_HAS_PATH
echo.
echo  ============================================
echo   正在部署插件到: %SAVED_PROJECT_DIR%
echo  ============================================
echo.
powershell -ExecutionPolicy Bypass -File "%TOOLS%\deploy_ue_plugin.ps1" -ProjectDir "%SAVED_PROJECT_DIR%"
if errorlevel 1 (
    echo.
    echo  [错误] 插件部署失败，请检查上方输出。
) else (
    echo.
    echo  [完成] TAAgent 插件集已部署！
    echo  现在打开你的 UE 项目，引擎会自动编译插件。
)
echo.
pause
goto MENU

:: ============================================================
:: [3] 启动 Unreal MCP 服务
:: ============================================================
:DO_START_UNREAL_MCP
cls
echo.
echo  ============================================
echo   启动 Unreal MCP 服务
echo   (保持此窗口打开，关闭窗口即停止服务)
echo  ============================================
echo.
echo  提示：请确保 UE 项目已打开且插件已加载。
echo  按 Ctrl+C 可停止服务。
echo.
powershell -ExecutionPolicy Bypass -File "%TOOLS%\start_unreal_mcp.ps1"
echo.
echo  服务已停止。
pause
goto MENU

:: ============================================================
:: [4] 生成 UE Python Console 粘贴代码
:: ============================================================
:DO_SHOW_SNIPPET
cls
echo.
echo  ============================================
echo   下面是需要粘贴到 UE Python Console 的代码
echo  ============================================
echo.
echo  步骤：
echo    1. 在 UE 编辑器里打开 Window → Developer Tools → Python Console
echo    2. 复制下面 ---- 之间的所有内容
echo    3. 粘贴到 Python Console 并回车执行
echo.
echo  ---- 复制开始 ----
echo.
powershell -ExecutionPolicy Bypass -File "%TOOLS%\show_unreal_python_console_snippet.ps1"
echo.
echo  ---- 复制结束 ----
echo.
pause
goto MENU

:: ============================================================
:: [5] 从 UE 项目卸载插件
:: ============================================================
:DO_REMOVE_PLUGIN
cls
echo.
if defined SAVED_PROJECT_DIR goto REMOVE_HAS_PATH
echo  你还没有设置 UE 项目路径。
echo.
set /p "SAVED_PROJECT_DIR=  请输入项目路径: "
set "SAVED_PROJECT_DIR=%SAVED_PROJECT_DIR:"=%"
if not defined SAVED_PROJECT_DIR (
    echo  未输入路径，返回菜单。
    timeout /t 2 >nul
    goto MENU
)
call :SAVE_CONFIG
:REMOVE_HAS_PATH
echo.
echo  ============================================
echo   正在从项目中移除插件: %SAVED_PROJECT_DIR%
echo  ============================================
echo.
powershell -ExecutionPolicy Bypass -File "%TOOLS%\remove_ue_plugin.ps1" -ProjectDir "%SAVED_PROJECT_DIR%"
if errorlevel 1 (
    echo.
    echo  [错误] 卸载失败，请检查上方输出。
) else (
    echo.
    echo  [完成] TAAgent 插件集已从你的项目中移除！
)
echo.
pause
goto MENU

:: ============================================================
:: [6] 启动 RenderDoc 本地隔离
:: ============================================================
:DO_OPEN_RENDERDOC
cls
echo.
echo  ============================================
echo   启动 RenderDoc (本地隔离模式)
echo  ============================================
echo.
if defined SAVED_RENDERDOC_EXE (
    call :NORMALIZE_RENDERDOC_PATH SAVED_RENDERDOC_EXE
    powershell -ExecutionPolicy Bypass -File "%TOOLS%\open_renderdoc_local.ps1" -RenderDocExe "%SAVED_RENDERDOC_EXE%"
) else (
    powershell -ExecutionPolicy Bypass -File "%TOOLS%\open_renderdoc_local.ps1"
)
if errorlevel 1 (
    echo.
    echo  [错误] RenderDoc 启动失败。
    echo  如果找不到 RenderDoc，请先用菜单 [9] 设置路径。
) else (
    echo.
    echo  [完成] RenderDoc 已启动。
)
echo.
pause
goto MENU

:: ============================================================
:: [7] 启动 RenderDoc MCP 服务
:: ============================================================
:DO_START_RENDERDOC_MCP
cls
echo.
echo  ============================================
echo   启动 RenderDoc MCP 服务
echo   (保持此窗口打开，关闭窗口即停止服务)
echo  ============================================
echo.
echo  提示：请先用 [6] 启动 RenderDoc 并打开一个 .rdc 文件。
echo  按 Ctrl+C 可停止服务。
echo.
powershell -ExecutionPolicy Bypass -File "%TOOLS%\start_renderdoc_mcp.ps1"
echo.
echo  服务已停止。
pause
goto MENU

:: ============================================================
:: [10] 触发当前 RenderDoc Live Capture 抓帧
:: ============================================================
:DO_TRIGGER_RENDERDOC_CAPTURE
cls
echo.
echo  ============================================
echo   触发当前 RenderDoc Live Capture 抓帧
echo  ============================================
echo.
echo  提示：
echo    1. 请先用 [6] 启动 RenderDoc
echo    2. 请先连到目标的 Live Capture 窗口
echo    3. 请等待该窗口显示 Established
echo.
powershell -ExecutionPolicy Bypass -File "%TOOLS%\trigger_renderdoc_live_capture.ps1"
if errorlevel 1 (
    echo.
    echo  [错误] 触发抓帧失败。
    echo  请确认当前打开的是目标进程的 Live Capture 窗口，且连接状态已经是 Established。
) else (
    echo.
    echo  [完成] 已向当前 Live Capture 窗口发送抓帧指令。
)
echo.
pause
goto MENU

:: ============================================================
:: [8] 设置 UE 项目路径
:: ============================================================
:DO_SET_PROJECT
cls
echo.
echo  ============================================
echo   设置 UE 项目路径
echo  ============================================
echo.
if defined SAVED_PROJECT_DIR (
    echo  当前路径: %SAVED_PROJECT_DIR%
)
echo.
echo  请输入包含 .uproject 文件的文件夹路径。
echo  (直接拖拽文件夹到这里也可以)
echo.
set "NEW_DIR="
set /p "NEW_DIR=  新路径: "
if not defined NEW_DIR (
    echo  未输入，保持原设置。
    timeout /t 2 >nul
    goto MENU
)
:: Remove surrounding quotes if user dragged a folder
set "NEW_DIR=%NEW_DIR:"=%"
if not exist "%NEW_DIR%" (
    echo.
    echo  [错误] 路径不存在: %NEW_DIR%
    pause
    goto MENU
)
set "SAVED_PROJECT_DIR=%NEW_DIR%"
call :SAVE_CONFIG
echo.
echo  [完成] UE 项目路径已保存为: %SAVED_PROJECT_DIR%
echo.
pause
goto MENU

:: ============================================================
:: [9] 设置 RenderDoc 路径
:: ============================================================
:DO_SET_RENDERDOC
cls
echo.
echo  ============================================
echo   设置 RenderDoc 可执行文件路径
echo  ============================================
echo.
if defined SAVED_RENDERDOC_EXE (
    echo  当前路径: %SAVED_RENDERDOC_EXE%
)
echo.
echo  请输入 qrenderdoc.exe 的完整路径。
echo  (直接拖拽文件到这里也可以)
echo.
set "NEW_RDC="
set /p "NEW_RDC=  RenderDoc路径: "
if not defined NEW_RDC (
    echo  未输入，保持原设置。
    timeout /t 2 >nul
    goto MENU
)
set "NEW_RDC=%NEW_RDC:"=%"
call :NORMALIZE_RENDERDOC_PATH NEW_RDC
if not exist "%NEW_RDC%" (
    echo.
    echo  [错误] 文件不存在: %NEW_RDC%
    pause
    goto MENU
)
if exist "%NEW_RDC%\*" (
    echo.
    echo  [错误] 请输入 qrenderdoc.exe 的完整路径，或输入包含 qrenderdoc.exe 的 RenderDoc 文件夹。
    pause
    goto MENU
)
set "SAVED_RENDERDOC_EXE=%NEW_RDC%"
call :SAVE_CONFIG
echo.
echo  [完成] RenderDoc 路径已保存为: %SAVED_RENDERDOC_EXE%
echo.
pause
goto MENU

:: ============================================================
:: 规范化 RenderDoc 路径
:: - 去掉拖拽时带入的引号
:: - 去掉尾部斜杠，避免传给 PowerShell 时截坏引号
:: - 如果传入的是目录，则自动补成 qrenderdoc.exe / renderdocui.exe
:: ============================================================
:NORMALIZE_RENDERDOC_PATH
setlocal EnableDelayedExpansion
call set "VALUE=%%%~1%%"
set "VALUE=%VALUE:"=%"
if defined VALUE if "!VALUE:~-1!"=="\" set "VALUE=!VALUE:~0,-1!"
if defined VALUE if "!VALUE:~-1!"=="/" set "VALUE=!VALUE:~0,-1!"
if exist "!VALUE!\qrenderdoc.exe" set "VALUE=!VALUE!\qrenderdoc.exe"
if not exist "!VALUE!" if exist "!VALUE!\renderdocui.exe" set "VALUE=!VALUE!\renderdocui.exe"
endlocal & set "%~1=%VALUE%"
exit /b

:: ============================================================
:: [0] 完全卸载
:: ============================================================
:DO_FULL_UNINSTALL
cls
echo.
echo  ============================================
echo   完全卸载 TAAgent
echo  ============================================
echo.
echo  这将执行以下操作:
echo.
if defined SAVED_PROJECT_DIR (
    echo    1. 从你的 UE 项目中移除 TAAgent 插件集
    echo       (%SAVED_PROJECT_DIR%\Plugins\UnrealMCP, AssetValidation 等)
) else (
    echo    1. (未设置 UE 项目路径，跳过插件移除)
)
echo    2. 删除 TAAgent 本地运行目录 (.taagent-local)
echo.
echo  操作完成后你可以直接删除整个 TAAgent 文件夹，
echo  不会在系统、引擎或项目中留下任何残留。
echo.
set "CONFIRM="
set /p "CONFIRM=  确认卸载？输入 Y 继续: "
if /i not "%CONFIRM%"=="Y" (
    echo  已取消。
    timeout /t 2 >nul
    goto MENU
)

echo.

:: Step 1: Remove plugin from project
if defined SAVED_PROJECT_DIR (
    echo  正在从项目中移除插件...
    powershell -ExecutionPolicy Bypass -File "%TOOLS%\remove_ue_plugin.ps1" -ProjectDir "%SAVED_PROJECT_DIR%"
    echo.
)

:: Step 2: Remove local runtime
if exist "%RUNTIME%" (
    echo  正在删除本地运行目录 (.taagent-local)...
    rmdir /s /q "%RUNTIME%"
    echo  已删除。
) else (
    echo  本地运行目录不存在，跳过。
)

echo.
echo  ============================================
echo  卸载完成！
echo.
echo  你现在可以直接删除整个 TAAgent 文件夹：
echo    %TAAGENT_ROOT%
echo.
echo  你的引擎、项目、系统环境都已恢复原样。
echo  ============================================
echo.
pause
goto MENU

:: ============================================================
:: 保存用户配置
:: ============================================================
:SAVE_CONFIG
if not exist "%RUNTIME%\config" (
    mkdir "%RUNTIME%\config" >nul 2>&1
)
(
    echo PROJECT_DIR=%SAVED_PROJECT_DIR%
    echo RENDERDOC_EXE=%SAVED_RENDERDOC_EXE%
) > "%CONFIG_FILE%"
exit /b

