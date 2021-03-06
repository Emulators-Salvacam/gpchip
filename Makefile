all : gpchip.dge

gpchip.dge: main.o
	mipsel-linux-gcc -o gpchip.dge main.o -lSDL -lSDL_ttf -lSDL_image -lSDL_mixer -Wall -O3 -lSDLmain -s -o gpchip.dge

main.o: main.c
	mipsel-linux-gcc main.c -c -o main.o 
