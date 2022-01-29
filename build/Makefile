MainCode='../code/linux-sdl-schmasteroids.cpp'
MainExe='linux_schmasteroids' 
SDLFlags=$(shell sdl2-config --cflags --libs)
MyFlags=-msse -msse2
linux_schmasteroids:
	c++ $(MainCode) -o $(MainExe) $(MyFlags) -DSCHM_SDL $(SDLFlags) -I ../code/
