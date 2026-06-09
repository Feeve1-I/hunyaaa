@echo off
chcp 65001 >nul
echo [*] Установка зависимостей (pymem)...
pip install -r requirements.txt
echo.
echo [*] Запуск memory_reader.py...
python memory_reader.py
pause
