#include "macro.h"
#include <gtk/gtk.h>

enum state {STATE_IDLE, STATE_RECORDING, STATE_PLAYING};

static struct event_sequence eventSequence;

static struct {
    
    struct event *playCurrentEvent;
    
    struct libevdev *inputDevice;
    struct libevdev_uinput *uinputDevice;
    
    GtkWidget *window;
    
    enum state state;
    GtkWidget *stateLabel;
    
    GtkWidget *evdevPathEntry;
    
    GtkWidget *recordButton;
    GtkWidget *playButton;
    
    GtkWidget *recordIcon;
    GtkWidget *playIcon;
    
    guint main_app_idle_id;
    
} appState = {
    .playCurrentEvent = NULL,
    .inputDevice = NULL,
    .uinputDevice = NULL,
    .window = NULL,
    .state = STATE_IDLE,
    .stateLabel = NULL,
    .evdevPathEntry = NULL,
    .recordButton = NULL,
    .playButton = NULL,
    .recordIcon = NULL,
    .playIcon = NULL,
    .main_app_idle_id = 0
};


static void cleanup() {
    event_sequence_destroy(&eventSequence);
    
    int fd;
    
    if(appState.inputDevice != NULL) {
    
        fd = libevdev_get_fd(appState.inputDevice);
        libevdev_free(appState.inputDevice);
        appState.inputDevice = NULL;
        if(fd != -1) {
            close(fd);
        }
    }
    
    if(appState.uinputDevice != NULL) {
    
        fd = libevdev_uinput_get_fd(appState.uinputDevice);
        libevdev_uinput_destroy(appState.uinputDevice);
        appState.uinputDevice = NULL;
        if(fd != -1) {
            close(fd);
        }
    }
}

static int init_dev(int fd) {
    
    int ret;
    
    if(fd != -1) {
    
        ret = libevdev_new_from_fd(fd, &appState.inputDevice);
        
        if(ret < 0) {
            errno = -ret;
            perror("Failed opening input device");
            return -1;
        }
    } else {
        appState.inputDevice = libevdev_new();
    
        libevdev_set_name(appState.inputDevice, "Custom Device");
    }
    
    int uidevfd = open("/dev/uinput", O_WRONLY);
    
    if(uidevfd == -1) {
        perror("Failed opening uinput device");
        close(uidevfd);
        return -1;
    }
    
    ret = libevdev_uinput_create_from_device(appState.inputDevice, uidevfd, &appState.uinputDevice);
    
    if(ret < 0) {
        errno = -ret;
        perror("Failed initializing uinput device");
        close(uidevfd);
        return -1;
    }
    
    return 0;
}

void show_error_dialog(const char* title, const char* details) {
    GtkAlertDialog *dialog = gtk_alert_dialog_new(title);
    
    gtk_alert_dialog_set_detail(dialog, details);
    gtk_alert_dialog_set_modal(dialog, TRUE);
    gtk_alert_dialog_show(dialog, GTK_WINDOW(appState.window));
}

static void record_tick() {
    static struct timespec lastEventTS = {.tv_sec = 0, .tv_nsec = 0};
    
    struct event event;
    
    if(!libevdev_has_event_pending(appState.inputDevice)) return;
    
    int ret = libevdev_next_event(appState.inputDevice, LIBEVDEV_READ_FLAG_NORMAL, &event.event);
    if(ret != LIBEVDEV_READ_STATUS_SUCCESS) {
        errno = EAGAIN;
        perror("Failed reading the event input device");
        return;
    }
    
    struct timespec currentTS;
    if(timespec_get(&currentTS, TIME_UTC) == 0) {
        perror("Failed getting current time");
    }
    
    if(eventSequence.tail != NULL) {
        event.delay.tv_sec = currentTS.tv_sec - lastEventTS.tv_sec;
        int32_t temp = (currentTS.tv_nsec - lastEventTS.tv_nsec);
        if(temp < 0 && event.delay.tv_sec > 0) {
            event.delay.tv_sec--;
            temp += 999999999;
        }
        event.delay.tv_nsec = temp;
    }
    
    lastEventTS = currentTS;
    
    ret = event_sequence_add_tail(&eventSequence, &event);
    
    if(ret == -1) {
        perror("Failed saving the input event");
        return;
    }
    
    event_print_info(stderr, &event);
    
}

static gboolean main_app_tick(gpointer user_data) {
    switch(appState.state) {
        case STATE_IDLE:
            gtk_label_set_label(GTK_LABEL(appState.stateLabel), "Current application state: IDLE");
            break;
        case STATE_PLAYING:
            gtk_label_set_label(GTK_LABEL(appState.stateLabel), "Current application state: PLAYING");
            break;
        case STATE_RECORDING:
            gtk_label_set_label(GTK_LABEL(appState.stateLabel), "Current application state: RECORDING");
            record_tick();
            break;
        default:
            gtk_label_set_label(GTK_LABEL(appState.stateLabel), "Current application state: UNKNOWN STATE");
            break;
    }
    
    return G_SOURCE_CONTINUE;
}

static void on_record_button_toggle(GtkWidget *button, gpointer user_data) {
    
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == TRUE && (appState.inputDevice == NULL || libevdev_get_fd(appState.inputDevice) == -1)) {
        show_error_dialog("Failed recording events", "Event input device wasn't initialized or unknown error occurred");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appState.recordButton), FALSE);
        gtk_image_set_from_icon_name(GTK_IMAGE(appState.recordIcon), "media-record");
        return;
    }
    
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == TRUE) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appState.playButton), FALSE);
        appState.state = STATE_RECORDING;
        
        gtk_image_set_from_icon_name(GTK_IMAGE(appState.recordIcon), "media-playback-stop");
        gtk_image_set_from_icon_name(GTK_IMAGE(appState.playIcon), "media-playback-start");
    } else {
        appState.state = STATE_IDLE;
        
        gtk_image_set_from_icon_name(GTK_IMAGE(appState.recordIcon), "media-record");
    }
    
}

static void on_play_button_toggle(GtkWidget *button, gpointer user_data) {
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == TRUE) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appState.recordButton), FALSE);
        appState.state = STATE_PLAYING;
        
        gtk_image_set_from_icon_name(GTK_IMAGE(appState.playIcon), "media-playback-pause");
        gtk_image_set_from_icon_name(GTK_IMAGE(appState.recordIcon), "media-record");
    } else {
        appState.state = STATE_IDLE;
        
        gtk_image_set_from_icon_name(GTK_IMAGE(appState.playIcon), "media-playback-start");
    }
    
}

static void on_evdev_load_button(GtkWidget *button, gpointer user_data) {
    int fd = open(gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(appState.evdevPathEntry))), O_RDONLY);
    if(fd == -1) {
        show_error_dialog("Failed loading the event input device. Didn't manage to open evdev file", strerror(errno));
        return;
    }
    cleanup();
    if(init_dev(fd) == -1) {
        show_error_dialog("Failed loading the event input device. Didn't manage to initialize input and uinput devices", strerror(errno));
        return;
    }
}

static void on_clear_button(GtkWidget *button, gpointer user_data) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appState.recordButton), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appState.playButton), FALSE);
    
}

static void on_reset_button(GtkWidget *button, gpointer user_data) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appState.playButton), FALSE);
    
}

static gboolean on_window_close(GtkWidget *window, gpointer user_data) {
    if(appState.main_app_idle_id != 0) g_source_remove(appState.main_app_idle_id);
    
    return FALSE;
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkBuilder *builder = gtk_builder_new_from_file("ui/linuxmacro.ui");
    
    appState.window = GTK_WIDGET(gtk_builder_get_object(builder, "app_window"));
    g_signal_connect_swapped(appState.window, "close-request", G_CALLBACK(on_window_close), NULL);
    gtk_window_set_application(GTK_WINDOW(appState.window), app);
    
    appState.recordButton = GTK_WIDGET(gtk_builder_get_object(builder, "record_toggle_button"));
    appState.playButton = GTK_WIDGET(gtk_builder_get_object(builder, "play_toggle_button"));
    
    appState.recordIcon = GTK_WIDGET(gtk_builder_get_object(builder, "record_button_icon"));
    appState.playIcon = GTK_WIDGET(gtk_builder_get_object(builder, "play_button_icon"));
    
    appState.stateLabel = GTK_WIDGET(gtk_builder_get_object(builder, "state_label"));
    appState.state = STATE_IDLE;
    
    g_signal_connect(appState.recordButton, "toggled", G_CALLBACK(on_record_button_toggle), NULL);
    g_signal_connect(appState.playButton, "toggled", G_CALLBACK(on_play_button_toggle), NULL);
    
    g_signal_connect(GTK_WIDGET(gtk_builder_get_object(builder, "clear_button")), "clicked", G_CALLBACK(on_clear_button), NULL);
    g_signal_connect(GTK_WIDGET(gtk_builder_get_object(builder, "reset_button")), "clicked", G_CALLBACK(on_reset_button), NULL);
    
    appState.evdevPathEntry = GTK_WIDGET(gtk_builder_get_object(builder, "evdev_path_entry"));
    
    g_signal_connect(GTK_WIDGET(gtk_builder_get_object(builder, "evdev_load_button")), "clicked", G_CALLBACK(on_evdev_load_button), NULL);
    
    appState.main_app_idle_id =  g_idle_add(main_app_tick, NULL);
    
    gtk_window_present(GTK_WINDOW(appState.window));
    
    g_object_unref(builder);
}



void signal_handler(int sig) {
    printf("Caught signal %d\n", sig);
    cleanup();
    _exit(1);
}

int main(int argc, char *argv[]) {
    
    event_sequence_init(&eventSequence);
    
    atexit(cleanup);
    signal(SIGINT, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGTERM, signal_handler);
    
    init_dev(-1);
    
    GtkApplication *app = gtk_application_new("dev.allivka.linuxmacro", G_APPLICATION_DEFAULT_FLAGS);
    
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    g_object_unref(app);
    
    cleanup();
    
    return status;
}