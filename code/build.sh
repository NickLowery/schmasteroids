mkdir -p build
pushd build
SOCode='../code/schmasteroids_main.cpp'
SOOut='schmasteroids.so'
MainCode='../code/linux-sdl-schmasteroids.cpp'
MainExe='linux_schmasteroids' 
SDLFlags=$(sdl2-config --cflags --libs)
SharedFlags='-g -Ofast -DDEBUG_BUILD -DSCHM_SDL -Wall -Wno-unused-function -Wno-unused-variable -Werror -fno-rtti -fno-exceptions -ldl'
echo "build: Entering directory '$(pwd)'"
echo "Building $MainExe"
c++ $MainCode -o $MainExe $SharedFlags $SDLFlags -I ../code/
echo "Building $SOOut"
c++ $SOCode -shared -fPIC -o $SOOut $SharedFlags -I ../code/
echo "build: Leaving directory '$(pwd)'"
popd
