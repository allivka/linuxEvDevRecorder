#include "macro.h"

#define MAX_EVENTS 300

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
      printf("Successfully read event %d\n", i + 1);
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

    printf("\nEvent number %d:\n", i + 1);
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
