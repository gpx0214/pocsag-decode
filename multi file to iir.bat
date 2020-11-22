CD /D "%~dp0"

:start
if "%~1"=="" goto end

iir.exe %1

shift /1
goto start

:end
