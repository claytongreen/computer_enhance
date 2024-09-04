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

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

set FLAGS=/nologo /arch:AVX512 /Zi /W4 /FC
set CFLAGS=%FLAGS% /std:c11

rem call cl %FLAGS% /O2 ..\disasm\main.cpp /Fedisasm.exe
rem call cl %FLAGS%     ..\disasm\main.cpp /Fedisasm.exe
rem call cl %FLAGS%     ..\disasm\main-gui.cpp /Fedisasm-gui.exe /link C:\dev\raylib-4.5.0_win64_msvc16\lib\raylibdll.lib

rem call cl %CFLAGS%     ..\haversine.c /Fehaversine-debug.exe         /Fmhaversine-debug.map         /D_DEBUG
rem call cl %CFLAGS%     ..\haversine.c /Fehaversine-debug-profile.exe /Fmhaversine-debug-profile.map /D_DEBUG /DPROFILE=1
rem call cl %CFLAGS% /O2 ..\haversine.c /Fehaversine-release.exe
rem call cl %CFLAGS% /O2 ..\haversine.c /Fehaversine-release-profile.exe                                       /DPROFILE=1

rem call cl %CFLAGS%     ..\repitition.c /Ferep-debug.exe /Fmrep-debug.map
rem call cl %CFLAGS% /O2 ..\repitition.c /Ferep.exe /Fmrep.map

rem call nasm -f win64 -o nop.obj ..\nop.asm
rem call lib /nologo nop.obj
rem call cl %CFLAGS% ..\repasm.c /Ferepasm.exe /Fmrepasm.map /link nop.lib

rem call cl %CFLAGS% /O2 ..\probe.c /Feprobe.exe /Fmprobe.map

rem call nasm -f win64 -o read.obj ..\read.asm
rem call lib /nologo read.obj
rem call cl %CFLAGS% ..\readasm.c /Fereadasm.exe /Fmreadasm.map /link read.lib

rem call nasm -f win64 -o write.obj ..\write.asm
rem call lib /nologo write.obj
rem call cl %CFLAGS% ..\writeasm.c /Fewriteasm.exe /Fmwriteasm.map /link write.lib

rem call nasm -f win64 -o mov.obj ..\mov.asm
rem call lib /nologo mov.obj
rem call cl %CFLAGS% ..\movasm.c /Femovasm.exe /Fmmovasm.map /link mov.lib

call nasm -f win64 -o cache.obj ..\haversine\cache.asm
call lib /nologo cache.obj
call cl %CFLAGS% ..\haversine\cache.c /Fecache.exe /Fmcache.map /link cache.lib

popd

