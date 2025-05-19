#include <fcntl.h>
#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>

struct event {
    struct input_event event;
    struct timespec delay;
    struct event *next_event;
};

struct event_sequence {
    struct event *head;
    struct event *tail;
};

int event_init(struct event *ev, const struct input_event *input_ev, 
                const struct timespec *delay_time, struct event *next) {
    if (ev == NULL) return -1;
    
    if (input_ev != NULL) {
        ev->event = *input_ev;
    } else {
        memset(&ev->event, 0, sizeof(struct input_event));
    }
    
    if (delay_time != NULL) {
        ev->delay = *delay_time;
    } else {
        ev->delay.tv_sec = 0;
        ev->delay.tv_nsec = 0;
    }
    
    ev->next_event = next;
    
    return 0;
}

int event_sequence_init(struct event_sequence *seq) {
    if(seq == NULL) return -1;
    seq->head = NULL;
    seq->tail = NULL;
    return 0;
}

int event_sequence_add_tail(struct event_sequence *seq, const struct event *ev) {
    if(seq == NULL) return -1;
    
    struct event *temp = malloc(sizeof(struct event));
    
    if(temp == NULL) {
        errno = ENOMEM;
        return -1;
    }
    
    if(ev == NULL) {
        event_init(temp, NULL, NULL, NULL);
    } else {
        event_init(temp, &ev->event, &ev->delay, NULL);
    }
    
    if(seq->tail == NULL) {
        seq->head = seq->tail = temp;
    } else {
        seq->tail->next_event = temp;
        seq->tail = temp;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    
    usleep(500 * 1000);
    
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
     
     
    struct event *currentEvent = malloc(sizeof(struct event));
    
    if(currentEvent == NULL) {
        errno = ENOMEM;
        perror("Failed allocating memory for temporal event storage");
        exit(EXIT_FAILURE);
    }
    
    struct event_sequence events;
    event_sequence_init(&events);
    
    struct timespec start, now;
    
    for(int i = 0; i < 99; i++) {
        timespec_get(&start, TIME_UTC);
        
        ret = libevdev_next_event(inputDevice, LIBEVDEV_READ_FLAG_BLOCKING, &currentEvent->event);
        
        if (ret < 0) {
            fprintf(stderr, "Error reading event: %s\n", strerror(-ret));
            libevdev_free(inputDevice);
            close(inputfd);
            exit(EXIT_FAILURE);
        }
        
        if (ret == LIBEVDEV_READ_STATUS_SUCCESS) {
            timespec_get(&now, TIME_UTC);
            currentEvent->delay.tv_sec = now.tv_sec - start.tv_sec;
            currentEvent->delay.tv_nsec = now.tv_nsec - start.tv_nsec;
            if(event_sequence_add_tail(&events, currentEvent) == -1) {
                perror("Error happened while saving the event to linked list");
                exit(EXIT_FAILURE);
            }
            printf("Successfully read event %d\n", i);
        }
    }
    
    sleep(1);
    
    currentEvent = events.head;
    
    for(int i = 0; i < 99 && currentEvent != NULL; i++) {
        
        printf("\nWaiting for the next event delay to end so as to recall the next event\n");
        sleep(currentEvent->delay.tv_sec);
        sleep(currentEvent->delay.tv_nsec);
        
        printf("Recalling the event:\n\ttype: %u(%s)\n\tcode: %u(%s)\n\tvalue: %i(%s)\n\tdelay seconds: %d\n\tdelay nanoseconds: %d\n\n", currentEvent->event.type, libevdev_event_type_get_name(currentEvent->event.type), currentEvent->event.code, libevdev_event_code_get_name(currentEvent->event.type, currentEvent->event.code), currentEvent->event.value, libevdev_event_value_get_name(currentEvent->event.type, currentEvent->event.code, currentEvent->event.value), currentEvent->delay.tv_sec, currentEvent->delay.tv_nsec);
        currentEvent = currentEvent->next_event;
    }
    
}