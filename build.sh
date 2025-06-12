#/bin/sh
gcc -o evdevrecorder src/main.c $(pkg-config --cflags --libs libevdev gtk4)
