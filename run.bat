@echo off
chcp 65001 >nul
title Codeforces Analyzer
setlocal enabledelayedexpansion

:menu
cls
echo ===============================
echo    Codeforces 用户分析工具
echo ===============================
echo.
echo 1. 单人模式 - 分析指定用户
echo 2. 多用户模式 - 使用默认用户列表
echo 3. 退出
echo.
set /p choice=请选择操作 (1/2/3):

if "%choice%"=="1" (
    set /p uname=请输入用户名:
    echo 分析用户: !uname!
    bin\cf-analyzer.exe -u !uname!
    echo.
    pause
    goto menu
)

if "%choice%"=="2" (
    bin\cf-analyzer.exe
    echo.
    pause
    goto menu
)

if "%choice%"=="3" (
    exit /b
)

echo 无效输入，请重新选择
pause
goto menu