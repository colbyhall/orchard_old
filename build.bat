@echo off

REM This is a basic build
set BASE_PATH="%~dp0"
cd %BASE_PATH%

call setup_cl.bat

if not exist bin mkdir bin

pushd bin

del *.exe
del *.pdb
del *.dll

set debug=/Zi /DDEBUG_BUILD
set release=/O2 /Zi /DRELEASE_BUILD
set build_mode=%debug%
if "%1" == "release" (set mode=%release%)

set defines=/D_CRT_SECURE_NO_WARNINGS 
set includes=/I../deps/ 
set disabled_warnings=/wd4201 /wd4100 /wd4189 /wd4505 /wd4127 /wd4204 /wd4221
set output_settings=/GR- /fp:fast /fp:except /EHa- /Oi /GS- /Gs9999999
set error_settings=/FC /nologo /WX /W4 /diagnostics:caret

REM The final compiler options
set opts=%defines% %includes% %disabled_warnings% %error_settings% %build_mode%

REM The linker options
set links=-incremental:no -opt:ref "kernel32.lib" "user32.lib" "gdi32.lib" "opengl32.lib" "fast_obj.lib"

if not exist fast_obj.lib (
    REM Build Fast OBJ as seperate to avoid name collisions
    call cl /c /Fefast_obj "..\deps\fast_obj\fast_obj.c"
    call lib fast_obj.obj
)

REM Buld game module
call cl %opts% /Fegame_module "..\src\orchard.c" /LD /link %links% /PDB:game_module_%random%.pdb /EXPORT:tick_game /EXPORT:init_game /EXPORT:shutdown_game
del game_module.lib

REM Buld platform layer
call cl %opts% /Feorchard "..\src\platform_win32.c" /link %links%

del *.obj
del *.exp

popd