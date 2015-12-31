@setlocal
@set TMPEXE=Release\unix_server.exe
@if NOT EXIST %TMPEXE% goto NOEXE
@set TMPCMD=
:RPT
@if "%~1x" == "x" goto GOTCMD
@set TMPCMD=%TMPCMD% %1
@shift
@goto RPT
:GOTCMD

%TMPEXE% %TMPCMD%

@goto END

:NOEXE
@echo ERROR: Can NOT locate EXE %TMPEXE%! Check name, location, and FIX ME %0
@goto END

:END
