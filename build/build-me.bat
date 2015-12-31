@setlocal
@REM 20151231 - re-named tcp-tests - F:\Projects\tcp-tests - all SG links removed!
@REM 20141118 - 20131015 - 20120705 - fgio
@set TMPPRJ=tcp-tests
@set TMPBGN=%TIME%

@REM Depreciated options - require OLD multi-library simgear - LONG AGO - TODO: REMOVE ALL SOURCES
@set ADDSG=0
@set ADDMETAR=0

@set TMPLOG=bldlog-1.txt
@set TMPSRC=..

@set TMPBASE=F:\FG\18
@set TMPGEN=Visual Studio 10
@set TMPINST=%TMPBASE%\3rdParty
@set TMPSG=%TMPBASE%\install\msvc100\simgear

@REM Path to 3rdparty
@set TMP3RD=%TMPBASE%\3rdParty
@set TMPIPATH=%TMP3RD%\include
@set TMPLPATH=%TMP3RD%\lib

@REM Set OPTIONS, if any
@set TMPOPTS=
@if NOT "%ADDSG%x" == "1x" goto DNSG
@REM ===================================================
@set TMPOPTS=%TMPOPTS% -DCMAKE_PREFIX_PATH=%TMP3RD%
@set TMPOPTS=%TMPOPTS% -DUSE_SIMGEAR:BOOL=TRUE
@if NOT EXIST %TMPINST%\nul goto NOINST
@if NOT EXIST %TMPSG%\nul goto NOSG
@REM set TMPOPTS=%TMPOPTS% -DM3RDPARTY_INCS=%TMPSG%\include
@REM set TMPOPTS=%TMPOPTS% -DM3RDPARTY_LIBS=%TMPSG%\lib
@REM set TMPOPTS=%TMPOPTS% -DCMAKE_LIBRARY_PATH=%TMPLPATH%
@REM ===================================================
@if NOT "%ADDMETAR%x" == "1x" goto DNMETAR
@set TMPOPTS=%TMPOPTS% -DADD_METAR2:BOOL=TRUE
:DNMETAR
:DNSG

@if NOT EXIST %TMPSRC%\nul goto NOSRC

@call chkmsvc %TMPPRJ%

@echo Bgn %DATE% %TIME% to %TMPLOG%
@REM Restart LOG
@echo. > %TMPLOG%
@echo Bgn %DATE% %TIME% >> %TMPLOG%


@echo Building in folder %CD% >> %TMPLOG%

@REM Deal with CREATING the CMakeLists.txt file
@REM if NOT EXIST %TMPSRC%\CMakeLists.txt
@if EXIST build-cmake.bat (
@echo Generate cmake list files >> %TMPLOG%
@call build-cmake >> %TMPLOG%
) else (
@echo build-cmake.bat not found in %CD% >> %TMPLOG%
)

@REM Check for primary CMakeLists.txt file

@if NOT EXIST %TMPSRC%\CMakeLists.txt goto NOSRC2

@REM All looks ok to proceed with BUILD

@REM Setup any environment variables
@REM Establish MSVC + SDK
@REM call set-msvc-sdk >> %TMPLOG%
@REM echo NUM 2: ERRORLEVEL=%ERRORLEVEL% >> %TMPLOG%

@REM Set ENVIRONMENT, if any
@if NOT "%ADDSG%x" == "1x" goto DNSG2
@if NOT EXIST %TMPSG%\nul goto NOSGD
@if NOT EXIST %TMPSG%\lib\nul goto NOSGD2
@set SIMGEAR_DIR=%TMPSG%
@echo Have added environment SIMGEAR_DIR=%SIMGEAR_DIR% >> %TMPLOG%
:DNSG2

@REM Do cmake configure and generation

@echo Doing 'cmake %TMPSRC% %TMPOPTS% >> %TMPLOG%
cmake %TMPSRC% %TMPOPTS% >> %TMPLOG% 2>&1
@echo Done 'cmake %TMPSRC% %TMPOPTS% ERRORLEVEL=%ERRORLEVEL% >> %TMPLOG%
@if ERRORLEVEL 1 goto NOCM

@REM Do cmake Debug build

@echo Doing: 'cmake --build . --config Debug' >> %TMPLOG%
cmake --build . --config Debug >> %TMPLOG%
@echo Done: 'cmake --build . --config Debug' ERRORLEVEL=%ERRORLEVEL% >> %TMPLOG%
@if ERRORLEVEL 1 goto NOBLD

@REM Do cmake Release build

@echo Doing: 'cmake --build . --config Release' >> %TMPLOG%
cmake --build . --config Release >> %TMPLOG% 2>&1
@echo Done: 'cmake --build . --config Release' ERRORLEVEL=%ERRORLEVEL% >> %TMPLOG%
@if ERRORLEVEL 1 goto NOBLD

@call elapsed %TMPBGN% >> %TMPLOG%
@echo End %DATE% %TIME% ERRORLEVEL=%ERRORLEVEL% >> %TMPLOG%
@call elapsed %TMPBGN%
@echo End %DATE% %TIME% - Success
@echo.
@echo No install at this time...
@echo.

@goto END

:NOSGD
@echo ERROR: %SIMGEAR_DIR% directory does NOT exist!
@goto ISERR

:NOSGD2
@echo ERROR: %SIMGEAR_DIR%\lib directory does NOT exist!
@goto ISERR

:NOSRC
@echo ERROR: Can NOT locate %TMPSRC%
@goto ISERR

:NOSRC2
@echo ERROR: Can NOT locate %TMPCMF%
@goto ISERR

:NOINST
@echo ERROR: Can NOT locate %TMPINST%
@goto ISERR

:NOSG
@echo ERROR: Can NOT locate %TMPSG%
@goto ISERR

:NOCM
@echo ERROR: CMake configure and generation error! ERRORLEVEL=%ERRORLEVEL%! >> %TMPLOG%
@goto ISERR

:NOBLD
@echo ERROR: CMake build error! ERRORLEVEL=%ERRORLEVEL%! >> %TMPLOG%
@goto ISERR

:ISERR
@echo ERROR EXIT - See LOG %TMPLOG%
@endlocal
@exit /b 1

:END
@endlocal
@exit /b 0

@REM eof
