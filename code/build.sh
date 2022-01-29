mkdir -p build
pushd build
MainCode='../code/linux-sdl-schmasteroids.cpp'
MainExe='linux_schmasteroids' 
SDLFlags=$(sdl2-config --cflags --libs)
MyFlags='-ffile-prefix-map=../code=./code \
         -fdebug-prefix-map=../code/./code \
         -g'
echo "build: Entering directory '/home/nlowery/projects/schmasteroids/build'"
c++ $MainCode -o $MainExe -DSCHM_SDL $SDLFlags -I ../code/
echo "build: Leaving directory '/home/nlowery/projects/schmasteroids/build'"
popd
