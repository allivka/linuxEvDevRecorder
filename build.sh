#/bin/sh
gcc -o macro src/main.c $(pkg-config --cflags --libs libevdev gtk4)