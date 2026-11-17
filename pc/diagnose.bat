@echo off
cd /d "%~dp0"
call ".venv\Scripts\activate.bat"

echo === Python / dependencies === > diagnose_log.txt
python -c "import sys, cv2, yaml, serial; print(sys.executable); print('cv2', cv2.__version__); print('yaml', yaml.__version__); print('serial', serial.__version__)" >> diagnose_log.txt 2>&1

echo. >> diagnose_log.txt
echo === Camera devices === >> diagnose_log.txt
python tools\list_cameras.py >> diagnose_log.txt 2>&1

echo. >> diagnose_log.txt
echo === main.py no serial 3 frames === >> diagnose_log.txt
python main.py --no-serial --no-show --max-frames 3 >> diagnose_log.txt 2>&1

echo. >> diagnose_log.txt
echo === main.py with serial 3 frames === >> diagnose_log.txt
python main.py --no-show --max-frames 3 >> diagnose_log.txt 2>&1

type diagnose_log.txt
echo.
echo Saved to: %~dp0diagnose_log.txt
pause