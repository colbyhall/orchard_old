@echo off

REM @NOTE(colby): This is a basic build
set BASE_PATH="%~dp0"
cd %BASE_PATH%

call setup_cl.bat

if not exist bin mkdir bin

pushd bin

del *.exe
del *.pdb
del *.dll

set debug=/Zi
set release=/O2 /Zi
set mode=%debug%
if "%1" == "release" (set mode=%release%)

set opts=/I../deps/ /D_CRT_SECURE_NO_WARNINGS -diagnostics:column -WL
set opts=%opts% -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4201 -wd4100 
set opts=%opts% -wd4189 -wd4505 -wd4127 -wd4204 -wd4221 -FC -GS- -Gs9999999 %mode%
set links=-incremental:no -opt:ref "kernel32.lib" "user32.lib" "gdi32.lib" "opengl32.lib"

REM @NOTE(colby): Buld game module
call cl %opts% -Fegame_module "..\src\orchard.c" -LD /link %links% -PDB:game_module_%random%.pdb -EXPORT:tick_game -EXPORT:init_game -EXPORT:shutdown_game

REM @NOTE(colby): Buld platform layer
call cl %opts% -Feorchard "..\src\platform_win32.c" /link %links%

del *.obj
del *.exp
del *.lib

popd