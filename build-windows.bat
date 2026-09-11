@echo off
setlocal
where make >nul 2>nul
if errorlevel 1 (
  echo Gebruik een MSYS2/MinGW-w64 omgeving met make, gcc, g++, nasm, sh en Python.
  exit /b 1
)
make windows CC=gcc WINCXX=g++ WINAR=ar WINDRES=windres PYTHON=python
if errorlevel 1 exit /b 1
echo Gereed: dist\atv-contestnummer.exe
