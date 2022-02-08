@echo off
set Mode=HSL_DebugOptimized

set Preprocess=No

if %Mode%==RenderBenchmark (
    set SharedCompilerFlags=-Z7 -FC -MT -nologo -GR- -O2 -Oi -W4 -WX -wd4189 -wd4201 -wd4100 -wd4505 ^
-DDEBUG_BUILD=1 -DRENDER_BENCHMARK=1 -DBENCHMARK=1 -DHSL_COLOR=1
) else if %Mode%==RunGameBenchmark (
    set SharedCompilerFlags=-Z7 -FC -MT -nologo -GR- -O2 -Oi -W4 -WX -wd4189 -wd4201 -wd4100 -wd4505 ^
-DDEBUG_BUILD=1 -DRUN_GAME_BENCHMARK=1 -DBENCHMARK=1 -DHSL_COLOR=1
) else if %Mode%==Release (
    set SharedCompilerFlags=-Z7 -FC -MT -nologo -GR- -O2 -Oi -W4 -WX -wd4189 -wd4201 -wd4100 -wd4505 ^
-DHSL_COLOR=1
) else if %Mode%==HSL_Debug (
    set SharedCompilerFlags=-Z7 -FC -MT -nologo -GR- -Od -Oi -W4 -WX -wd4189 -wd4201 -wd4100 -wd4505 ^
-DDEBUG_BUILD=1 -DASSERTIONS=1 -DHSL_COLOR=1
) else if %Mode%==RenderBenchmarkUnoptimized (
    set SharedCompilerFlags=-Z7 -FC -MT -nologo -GR- -Od -Oi -W4 -WX -wd4189 -wd4201 -wd4100 -wd4505 ^
-DDEBUG_BUILD=1 -DRENDER_BENCHMARK=1 -DBENCHMARK=1 -DHSL_COLOR=1 -DASSERTIONS=1
) else if %Mode%==Debug (
    set SharedCompilerFlags=-Z7 -FC -MT -nologo -GR- -Od -Oi -W4 -WX -wd4189 -wd4201 -wd4100 -wd4505 ^
-DDEBUG_BUILD=1 -DASSERTIONS=1
) else if %Mode%==HSL_DebugOptimized (
    set SharedCompilerFlags=-Z7 -Zo -FC -MT -nologo -GR- -O2 -Oi -W4 -WX -wd4189 -wd4201 -wd4100 -wd4505 ^
-DDEBUG_BUILD=1 -DASSERTIONS=1 -DHSL_COLOR=1
) else if %Mode%==CreateGameBenchmark (
    set SharedCompilerFlags=-Z7 -FC -MT -nologo -GR- -O2 -Oi -W4 -WX -wd4189 -wd4201 -wd4100 -wd4505 ^
-DDEBUG_BUILD=1 -DCREATE_GAME_BENCHMARK=1 -DHSL_COLOR=1
) else if %Mode%==RunGameBenchmarkUnoptimized (
    set SharedCompilerFlags=-Z7 -FC -MT -nologo -GR- -Od -Oi -W4 -WX -wd4189 -wd4201 -wd4100 -wd4505 ^
-DDEBUG_BUILD=1 -DASSERTIONS=1 -DRUN_GAME_BENCHMARK=1 -DBENCHMARK=1 -DHSL_COLOR=1
)

set SharedLinkerFlags=-opt:ref -incremental:no gdi32.lib user32.lib Ole32.lib winmm.lib
IF NOT EXIST build (mkdir build)
pushd build
set dt=%date:~4,2%%date:~7,2%%date:~10,4%__%time:~0,2%_%time:~3,2%_%time:~6,2%_%time:~9,2%
set dt=%dt: =_%

if %Preprocess%==Yes (
    cl /C /P /EP /Fipreprocessed.cpp %SharedCompilerFlags% ..\code\schmasteroids_main.cpp
    set MainSource=preprocessed.cpp
) else (
    set MainSource=..\code\schmasteroids_main.cpp
)

del *.pdb > NUL 2> NUL
echo WAITING FOR PDB > lock.tmp

cl %SharedCompilerFlags% -LD %MainSource%  /link -incremental:no -opt:ref -PDB:schmasteroids_main_%dt%.pdb /DLL /EXPORT:GameGetSoundOutput /EXPORT:GameUpdateAndRender /EXPORT:GameInitialize /EXPORT:SeedRandom

del lock.tmp
cl %SharedCompilerFlags% ..\code\win32_schmasteroids.cpp /link %SharedLinkerFlags%
popd
