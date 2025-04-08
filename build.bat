@echo off
set exe_name=chip8.exe
set c_file=main.c
:: WINDOWS advanced build command for debugging
clang %c_file% -g -gcodeview -Wl,--pdb= windows/lib/libraylib.a -lopengl32 -lgdi32 -lwinmm -I /include  -o %exe_name%

echo %exe_name% was built successfully
