radio: main.cpp
	g++ -framework Cocoa -framework OpenGL -lglfw -lrtlsdr -lfftw3 -lm -o radio main.cpp