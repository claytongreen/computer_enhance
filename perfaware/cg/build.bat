@echo off

IF NOT EXIST build mkdir build
pushd build

call cl /nologo /Zi ..\main.cpp /Fedisasm.exe

popd
