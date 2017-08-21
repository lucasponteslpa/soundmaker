#
#  Configure:
#  source /opt/yocto/poky/1.3.2/environment-setup-core2-poky-linux
#

app: app.cpp
	g++ `sdl2-config --cflags` -o app app.cpp LTexture.cpp senos.o -fopenmp -lasound -lm -L/usr/include/alsa `sdl2-config --libs` -std=c++11 -lSDL2_image -lSDL2_mixer -lSDL2_ttf 
	# cp app /opt/tftp

clean:
	rm -rf *.o *~ core app
