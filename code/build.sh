mkdir -p build
pushd build
SOCode='../code/schmasteroids_main.cpp'
SOOut='schmasteroids.so'
MainCode='../code/linux-sdl-schmasteroids.cpp'
MainExe='linux_schmasteroids' 
SDLFlags=$(sdl2-config --cflags --libs)
SharedFlags='-g -DDEBUG_BUILD -DSCHM_SDL -Wall -Wno-unused-function -Wno-unused-variable -Werror -fno-rtti -fno-exceptions'
echo "build: Entering directory '/home/nlowery/projects/schmasteroids/build'"
echo "Building $MainExe"
c++ $MainCode -o $MainExe $SharedFlags $SDLFlags -I ../code/
echo "Building $SOOut"
c++ $SOCode -shared -o $SOOut $SharedFlags -I ../code/
echo "build: Leaving directory '/home/nlowery/projects/schmasteroids/build'"
popd
