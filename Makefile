rtl_spectrum: main.c
	cc -framework Cocoa -framework OpenGL -lglfw -lrtlsdr -lfftw3 -lm -o rtl_spectrum main.c

clean:
	rm -f rtl_spectrum
