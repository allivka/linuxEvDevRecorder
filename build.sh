#/bin/sh
gcc -o macro main.c $(pkg-config --cflags --libs libevdev)