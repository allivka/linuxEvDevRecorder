#include <fcntl.h>
#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input-device>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int inputfd = open(argv[1], O_RDONLY);
    if (inputfd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    struct libevdev *inputDevice;
    int ret = libevdev_new_from_fd(inputfd, &inputDevice);
    if (ret < 0) {
        fprintf(stderr, "Failed to init libevdev: %s\n", strerror(-ret));
        close(inputfd);
        exit(EXIT_FAILURE);
    }
    
    struct input_event events[99];
    int event_count = 0;

    while (event_count < 99) {
        struct input_event currentEvent;
        ret = libevdev_next_event(inputDevice, LIBEVDEV_READ_FLAG_BLOCKING, &currentEvent);
        if (ret < 0) {
            fprintf(stderr, "Error reading event: %s\n", strerror(-ret));
            libevdev_free(inputDevice);
            close(inputfd);
            exit(EXIT_FAILURE);
        }
        if (ret == LIBEVDEV_READ_STATUS_SUCCESS) {
            events[event_count++] = currentEvent;
            printf("Successfully read event %d\n", event_count);
        }
    }

    sleep(3);

    int uinputfd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinputfd == -1) {
        perror("open /dev/uinput");
        libevdev_free(inputDevice);
        close(inputfd);
        exit(EXIT_FAILURE);
    }

    struct libevdev_uinput *outputDevice;
    ret = libevdev_uinput_create_from_device(inputDevice, uinputfd, &outputDevice);
    if (ret < 0) {
        fprintf(stderr, "Failed to create uinput device: %s\n", strerror(-ret));
        libevdev_free(inputDevice);
        close(inputfd);
        close(uinputfd);
        exit(EXIT_FAILURE);
    }

    libevdev_free(inputDevice);
    close(inputfd);

    for (int i = 0; i < 99; i++) {
        ret = libevdev_uinput_write_event(outputDevice, events[i].type, events[i].code, events[i].value);
        if (ret < 0) {
            fprintf(stderr, "Failed to write event: %s\n", strerror(-ret));
            libevdev_uinput_destroy(outputDevice);
            exit(EXIT_FAILURE);
        }
        printf("Event %d: type=%d, code=%d, value=%d\n", i + 1, events[i].type, events[i].code, events[i].value);
        usleep(500); 
    }
    
    libevdev_uinput_write_event(outputDevice, EV_SYN, SYN_REPORT, 0);
    
    sleep(1);
    libevdev_uinput_destroy(outputDevice);
}