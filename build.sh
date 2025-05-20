#/bin/sh
gcc -o test1 src/test1.c $(pkg-config --cflags --libs libevdev)
gcc -o macro src/main.c $(pkg-config --cflags --libs libevdev gtk4)