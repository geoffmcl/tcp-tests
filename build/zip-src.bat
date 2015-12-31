@setlocal
@if EXIST CMakeCache.txt goto NOWAY
@if EXIST ..\CMakeLists.txt.bak goto NOWAY
@set TMPVER=11
@set TMPZIP=fgio-%TMPVER%.zip
@set TMPDST=..\..\zips
@if NOT EXIST %TMPDST%\nul goto NODST
@set TMPSELF=..\..\fgio-%TMPVER%\build\zip-src.bat
@if NOT EXIST %TMPSELF% goto NOSELF
@cd ..\..

@set TMPFIL=zips\%TMPZIP%
@set TMPA=-a
@if EXIST %TMPFIL% (
@call dirmin %TMPFIL%
@echo This is an UPDATE
@set TMPA=-u
)

@echo Will do 'call zip8 %TMPA% -r -P -o %TMPFIL% fgio-%TMPVER%\*'
@echo *** CONTINUE? ***
@pause

call zip8 %TMPA% -r -P -o %TMPFIL% fgio-%TMPVER%\*

@goto END

:NODST
@echo Can NOT locate %TMPDST%! *** FIX ME ***
@goto END

:NOSELF
@echo Can NOT find SELF %TMPSELF%! *** FIX ME ***
@goto END

:NOWAY
@echo Found built components!!! Do bak and cmake clean first...

:END

