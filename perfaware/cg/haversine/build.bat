@echo off
setlocal enabledelayedexpansion

where /Q cl.exe || (
   set __VSCMD_ARG_NO_LOGO=1
   for /f "tokens=*" %%i in ('"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath') do set VS=%%i
   if "!VS!" equ "" (
     echo ERROR: Visual Studio installation not found
     exit /b 1
   )
   call "!VS!\VC\Auxiliary\Build\vcvarsall.bat" amd64 || exit /b 1
)

IF NOT EXIST build mkdir build
pushd build

set FLAGS=/nologo /std:c11 /arch:AVX2 /Zi /W4

rem call cl /nologo /Zi /O2 /FC ..\disasm\main.cpp /Fedisasm.exe
rem call cl /nologo /Zi /FC ..\disasm\main.cpp /Fedisasm.exe
rem call cl /nologo /Zi /FC ..\disasm\main-gui.cpp /Fedisasm-gui.exe /link C:\dev\raylib-4.5.0_win64_msvc16\lib\raylibdll.lib

rem call cl /nologo /std:c11 /arch:AVX2 /Zi /W4     ..\haversine.c /Fehaversine-debug.exe         /D_DEBUG
rem call cl %FLAGS%     ..\haversine.c /Fehaversine-debug-profile.exe /D_DEBUG /DPROFILE=1
rem call cl %FLAGS% /O2 ..\haversine.c /Fehaversine-release.exe
rem call cl %FLAGS% /O2 ..\haversine.c /Fehaversine-release-profile.exe        /DPROFILE=1

call cl %FLAGS% /O2 ..\repitition.c

popd

