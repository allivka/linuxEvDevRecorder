#include "macro.h"
#include <gtk/gtk.h>

enum state {STATE_RECORDING, STATE_PLAYING, STATE_IDLE};

struct {
    enum state state;
    GtkWidget *stateLabel;
    
    GtkWidget *recordButton;
    GtkWidget *playButton;
    
    GtkWidget *clearButton;
    GtkWidget *replayButton;
    
    GtkWidget *recordIcon;
    GtkWidget *playIcon;
    
    guint main_app_idle_id;
    
} appState;

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
            break;
        default:
            gtk_label_set_label(GTK_LABEL(appState.stateLabel), "Current application state: UNKNOW STATE");
            break;
    }
    
    return G_SOURCE_CONTINUE;
}

static void on_record_button_toggle(GtkWidget *button, gpointer user_data) {
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == TRUE) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appState.playButton), FALSE);
        appState.state = STATE_RECORDING;
        
        gtk_image_set_from_icon_name(GTK_IMAGE(appState.recordIcon), "process-stop");
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

static gboolean on_window_close(GtkWidget *window, gpointer user_data) {
    if(appState.main_app_idle_id != 0) g_source_remove(appState.main_app_idle_id);
    
    return FALSE;
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkBuilder *builder = gtk_builder_new_from_file("ui/linuxmacro.ui");
    
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "app_window"));
    g_signal_connect_swapped(window, "close-request", G_CALLBACK(on_window_close), NULL);
    gtk_window_set_application(GTK_WINDOW(window), app);
    
    appState.recordButton = GTK_WIDGET(gtk_builder_get_object(builder, "record_toggle_button"));
    appState.playButton = GTK_WIDGET(gtk_builder_get_object(builder, "play_toggle_button"));
    
    appState.recordIcon = GTK_WIDGET(gtk_builder_get_object(builder, "record_button_icon"));
    appState.playIcon = GTK_WIDGET(gtk_builder_get_object(builder, "play_button_icon"));
    
    appState.stateLabel = GTK_WIDGET(gtk_builder_get_object(builder, "state_label"));
    appState.state = STATE_IDLE;
    
    g_signal_connect(appState.recordButton, "toggled", G_CALLBACK(on_record_button_toggle), NULL);
    g_signal_connect(appState.playButton, "toggled", G_CALLBACK(on_play_button_toggle), NULL);
    
    appState.main_app_idle_id =  g_idle_add(main_app_tick, NULL);
    
    
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    
    GtkApplication *app = gtk_application_new("com.linuxMacro.allivka.github", G_APPLICATION_DEFAULT_FLAGS);
    
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    g_object_unref(app);
    
    return status;
}