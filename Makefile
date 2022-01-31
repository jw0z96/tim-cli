timpack: main.c timpack.c
	gcc -o timpack \
		main.c \
		timpack.c \
		-I/opt/homebrew/opt/argp-standalone/include \
		-L/opt/homebrew/opt/argp-standalone/lib \
		-largp \
		-Wall
