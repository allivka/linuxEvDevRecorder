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

#define MAX_EVENTS 99

struct event {
    struct input_event event;
    struct timespec delay;
    struct event *next_event;
};

struct event_sequence {
    struct event *head;
    struct event *tail;
};

int event_init(struct event *ev, const struct input_event *input_ev, const struct timespec *delay_time, struct event *next) {
    if (ev == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    memset(&ev->event, 0, sizeof(struct input_event));
    if (input_ev != NULL) {
        ev->event = *input_ev;
    }
    
    memset(&ev->delay, 0, sizeof(struct timespec));
    if (delay_time != NULL) {
        ev->delay = *delay_time;
    }
    
    ev->next_event = next;
    return 0;
}

int event_sequence_init(struct event_sequence *seq) {
    if (seq == NULL) {
        errno = EINVAL;
        return -1;
    }
    seq->head = NULL;
    seq->tail = NULL;
    return 0;
}

int event_sequence_free(struct event_sequence *seq) {
    if (seq == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    struct event *current = seq->head;
    while (current != NULL) {
        struct event *next = current->next_event;
        free(current);
        current = next;
    }
    
    seq->head = seq->tail = NULL;
    return 0;
}

int event_sequence_add_tail(struct event_sequence *seq, const struct event *ev) {
    if (seq == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    struct event *temp = malloc(sizeof(struct event));
    if (temp == NULL) {
        errno = ENOMEM;
        return -1;
    }
    
    int ret = event_init(temp, (ev != NULL) ? &ev->event : NULL,
                            (ev != NULL) ? &ev->delay : NULL, NULL);
    if (ret != 0) {
        free(temp);
        return -1;
    }
    
    if (seq->tail == NULL) {
        seq->head = seq->tail = temp;
    } else {
        seq->tail->next_event = temp;
        seq->tail = temp;
    }
    return 0;
}

int event_print_info(FILE *stream, const struct event *event) {
    if (event == NULL || stream == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    fprintf(stream,
        "Event info:\n\ttype: %u(%s)\n\tcode: %u(%s)\n\tvalue: %d(%s)\n"
        "\tdelay seconds: %ld\n\tdelay nanoseconds: %ld\n",
        event->event.type, 
        libevdev_event_type_get_name(event->event.type),
        event->event.code,
        libevdev_event_code_get_name(event->event.type, event->event.code),
        event->event.value,
        libevdev_event_value_get_name(event->event.type, event->event.code, event->event.value),
        event->delay.tv_sec,
        event->delay.tv_nsec
    );
}

int event_sequence_recall_to_uinput(const struct event_sequence *seq, const struct libevdev *source_device) {
    if (seq == NULL || source_device == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    int uinputfd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinputfd == -1) {
        perror("Failed to open uinput");
        return -1;
    }
    
    struct libevdev_uinput *uidev;
    int ret = libevdev_uinput_create_from_device(source_device, uinputfd, &uidev);
    if (ret != 0) {
        close(uinputfd);
        fprintf(stderr, "Failed to create uinput device: %s\n", strerror(-ret));
        return -1;
    }
    
    struct event *current = seq->head;
    while (current != NULL) {
        if (nanosleep(&current->delay, NULL) == -1) {
            if (errno != EINTR) {  // Handle signal interruptions
                close(uinputfd);
                libevdev_uinput_destroy(uidev);
                return -1;
            }
        }
        
        ret = libevdev_uinput_write_event(uidev, current->event.type,
                                         current->event.code, current->event.value);
        if (ret != 0) {
            close(uinputfd);
            libevdev_uinput_destroy(uidev);
            fprintf(stderr, "Failed to write event: %s\n", strerror(-ret));
            return -1;
        }
        
        current = current->next_event;
    }
    
    close(uinputfd);
    libevdev_uinput_destroy(uidev);
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

    struct libevdev *input_device;
    int ret = libevdev_new_from_fd(inputfd, &input_device);
    if (ret < 0) {
        fprintf(stderr, "Failed to init libevdev: %s\n", strerror(-ret));
        close(inputfd);
        exit(EXIT_FAILURE);
    }
    
    struct event_sequence events;
    if (event_sequence_init(&events) != 0) {
        perror("Failed to initialize event sequence");
        libevdev_free(input_device);
        close(inputfd);
        exit(EXIT_FAILURE);
    }
    
    struct timespec start, now;
    for (int i = 0; i < MAX_EVENTS; i++) {
        struct event *current_event = malloc(sizeof(struct event));
        if (current_event == NULL) {
            perror("Failed to allocate event");
            event_sequence_free(&events);
            libevdev_free(input_device);
            close(inputfd);
            exit(EXIT_FAILURE);
        }
        
        if (timespec_get(&start, TIME_UTC) != TIME_UTC) {
            perror("Failed to get start time");
            free(current_event);
            event_sequence_free(&events);
            libevdev_free(input_device);
            close(inputfd);
            exit(EXIT_FAILURE);
        }
        
        ret = libevdev_next_event(input_device, LIBEVDEV_READ_FLAG_BLOCKING, 
                                 &current_event->event);
        if (ret < 0) {
            fprintf(stderr, "Error reading event: %s\n", strerror(-ret));
            free(current_event);
            event_sequence_free(&events);
            libevdev_free(input_device);
            close(inputfd);
            exit(EXIT_FAILURE);
        }
        
        if (ret == LIBEVDEV_READ_STATUS_SUCCESS) {
            if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
                perror("Failed to get current time");
                free(current_event);
                event_sequence_free(&events);
                libevdev_free(input_device);
                close(inputfd);
                exit(EXIT_FAILURE);
            }
            
            current_event->delay.tv_sec = now.tv_sec - start.tv_sec;
            current_event->delay.tv_nsec = now.tv_nsec - start.tv_nsec;
            if (current_event->delay.tv_nsec < 0) {
                current_event->delay.tv_sec--;
                current_event->delay.tv_nsec += 1000000000;
            }
            
            if (event_sequence_add_tail(&events, current_event) == -1) {
                perror("Failed to add event to sequence");
                free(current_event);
                event_sequence_free(&events);
                libevdev_free(input_device);
                close(inputfd);
                exit(EXIT_FAILURE);
            }
            
            free(current_event);
            printf("Successfully read event %d\n", i+1);
        }
    }
    
    sleep(1);
    
    struct event *current = events.head;
    for (int i = 0; current != NULL; i++) {
        printf("\nWaiting for next event delay\n");
        struct timespec sleep_time = current->delay;
        if (nanosleep(&sleep_time, NULL) == -1 && errno != EINTR) {
            perror("nanosleep failed");
            event_sequence_free(&events);
            libevdev_free(input_device);
            close(inputfd);
            exit(EXIT_FAILURE);
        }
        
        printf("\nEvent number %d:\n", i+1);
        event_print_info(stdout, current);
        current = current->next_event;
    }
    
    usleep(500 * 1000);
    printf("\nStarted recalling events\n");
    if (event_sequence_recall_to_uinput(&events, input_device) != 0) {
        perror("Failed to recall events to uinput");
        event_sequence_free(&events);
        libevdev_free(input_device);
        close(inputfd);
        exit(EXIT_FAILURE);
    }
    printf("\nFinished recalling events\n");
    
    event_sequence_free(&events);
    libevdev_free(input_device);
    close(inputfd);
    
    return 0;
}