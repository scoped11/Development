@echo off
sc stop TsSvc
sc delete TsSvc
timeout /t 3 >nul
mkdir C:\DriverLab\map 2>nul
copy /Y Z:\byovd\bin\mapper_nal.exe C:\DriverLab\map\
copy /Y Z:\byovd\bin\RwTest.sys C:\DriverLab\map\
copy /Y Z:\byovd\vehicles\ThrottleStop.sys C:\DriverLab\map\
cd /d C:\DriverLab\map
echo START %DATE% %TIME% > C:\DriverLab\ts-out.txt
mapper_nal.exe RwTest.sys --driver ts --no-auth --rw-only --quiet --no-fallback >> C:\DriverLab\ts-out.txt 2>&1
echo EXIT %ERRORLEVEL% >> C:\DriverLab\ts-out.txt
echo DONE %TIME% >> C:\DriverLab\ts-out.txt
sc query TsSvc >> C:\DriverLab\ts-out.txt
