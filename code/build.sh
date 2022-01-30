mkdir -p build
pushd build
SOCode='../code/schmasteroids_main.cpp'
SOOut='schmasteroids.so'
MainCode='../code/linux-sdl-schmasteroids.cpp'
MainExe='linux_schmasteroids' 
SDLFlags=$(sdl2-config --cflags --libs)
SharedFlags='-g -DDEBUG_BUILD -DSCHM_SDL'
echo "build: Entering directory '/home/nlowery/projects/schmasteroids/build'"
c++ $MainCode -o $MainExe $SharedFlags $SDLFlags -I ../code/
c++ $SOCode -shared -o $SharedFlags -I ../code/
echo "build: Leaving directory '/home/nlowery/projects/schmasteroids/build'"
popd
