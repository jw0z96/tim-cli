timpack: timpack.c tim_io_utils.c
	gcc -o timpack \
		timpack.c \
		tim_io_utils.c \
		-I/opt/homebrew/opt/argp-standalone/include \
		-L/opt/homebrew/opt/argp-standalone/lib \
		-largp \
		-Wall

timview: timview.c tim_io_utils.c
	gcc -o timview \
		timview.c \
		tim_io_utils.c \
		-I/opt/homebrew/opt/sdl2/include \
		-L/opt/homebrew/opt/sdl2/lib \
		-lsdl2 \
		-Wall
