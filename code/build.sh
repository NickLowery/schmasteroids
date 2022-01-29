mkdir -p build
pushd build
MainCode='../code/linux-sdl-schmasteroids.cpp'
MainExe='linux_schmasteroids' 
SDLFlags=$(sdl2-config --cflags --libs)
c++ $MainCode -o $MainExe $SDLFlags
popd
