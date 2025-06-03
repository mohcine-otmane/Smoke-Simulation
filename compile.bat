@echo off
gcc smoke_simulation.c -o smoke_simulation -I"C:\SDL2\include" -L"C:\SDL2\lib" -lSDL2main -lSDL2 -lSDL2_image -lm 