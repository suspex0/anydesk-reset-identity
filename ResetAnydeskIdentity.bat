@echo off
set "programdata=%programdata%\AnyDesk"
set "appdata=%appdata%\AnyDesk"

echo Killing process..
taskkill /f /im AnyDesk.exe

echo Deleting files..
del /q "%programdata%\*"
del /q "%appdata%\*"

echo Done.
pause
