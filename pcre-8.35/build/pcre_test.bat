@REM This is a generated file.
@echo off
setlocal
SET srcdir="C:\Develop\code\ge\3rd\pcre-8.33"
SET pcretest="C:\Develop\code\ge\3rd\pcre-8.33\build\DEBUG\pcretest.exe"
if not [%CMAKE_CONFIG_TYPE%]==[] SET pcretest="C:\Develop\code\ge\3rd\pcre-8.33\build\%CMAKE_CONFIG_TYPE%\pcretest.exe"
call %srcdir%\RunTest.Bat
if errorlevel 1 exit /b 1
echo RunTest.bat tests successfully completed
