
#ifndef MACRO_HPP
#define MACRO_HPP

#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

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
          event->event.type, libevdev_event_type_get_name(event->event.type),
          event->event.code,
          libevdev_event_code_get_name(event->event.type, event->event.code),
          event->event.value,
          libevdev_event_value_get_name(event->event.type, event->event.code,
                                        event->event.value),
          event->delay.tv_sec, event->delay.tv_nsec);
}

int event_recall_to_uinput_no_delay(const struct event *event, const struct libevdev_uinput *uidev) {
  int ret = libevdev_uinput_write_event(uidev, event->event.type, event->event.code, event->event.value);
  if (ret != 0) {
    fprintf(stderr, "Failed to write event: %s\n", strerror(-ret));
    return -1;
  }
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
      if (errno != EINTR) {
        close(uinputfd);
        libevdev_uinput_destroy(uidev);
        return -1;
      }
    }

    ret = libevdev_uinput_write_event(uidev, current->event.type, current->event.code, current->event.value);
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

#endif