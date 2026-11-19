@echo off
cd /d "%~dp0"
call ".venv\Scripts\activate.bat"
echo Recording Vision-Link window in 3 seconds, up to 60 seconds...
python tools\record_window.py --duration 60 --output demo.mp4
echo.
echo Saved to: %~dp0demo.mp4
pause