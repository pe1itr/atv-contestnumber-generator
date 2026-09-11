@echo off
setlocal
where gcc >nul 2>nul
if errorlevel 1 (
  echo Installeer een MinGW-w64 compiler en voeg de bin-map toe aan PATH.
  exit /b 1
)
if not exist build mkdir build
if not exist dist mkdir dist
python tools/embed_pm5544.py
if errorlevel 1 exit /b 1
gcc -std=c11 -O2 -Wall -Wextra -Werror -Ibuild -c src/pm5544.c -o build/pm5544-windows.o
if errorlevel 1 exit /b 1
windres -Isrc src/app.rc build/app.o
if errorlevel 1 exit /b 1
gcc -std=c11 -O2 -Wall -Wextra -Werror -municode -mwindows -static -s src/main.c src/core.c src/app_info.c build/app.o build/pm5544-windows.o -o dist/atv-contestnummer.exe -lwindowscodecs -lole32 -loleaut32 -luuid -lgdi32 -lcomctl32 -lcomdlg32 -lbcrypt
if errorlevel 1 exit /b 1
echo Gereed: dist\atv-contestnummer.exe
