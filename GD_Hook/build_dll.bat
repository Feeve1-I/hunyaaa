@echo off
chcp 65001 >nul
echo [*] Генерация проекта CMake (x86 - 32-bit)...
cmake -S . -B build -A Win32
if %errorlevel% neq 0 (
    echo [!] Ошибка CMake. Убедитесь, что установлен CMake и Visual Studio (C++ workload).
    pause
    exit /b %errorlevel%
)

echo.
echo [*] Компиляция DLL (Release)...
cmake --build build --config Release
if %errorlevel% neq 0 (
    echo [!] Ошибка компиляции.
    pause
    exit /b %errorlevel%
)

echo.
echo [+] Успешно! DLL находится в папке: build\Release\SpeedhackHook.dll
pause
