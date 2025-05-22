#include "macro.h"
#include <gtk/gtk.h>

enum state {STATE_RECORDING, STATE_PLAYING, STATE_IDLE};

struct {
    enum state state;
    GtkWidget *recordButton;
    GtkWidget *playButton;
} appState;

static gboolean main_app_tick(gpointer user_data) {
    printf("\nCurrent state: ");
    switch(appState.state) {
        case STATE_IDLE:
            printf("idle");
            break;
        case STATE_PLAYING:
            printf("playing");
            break;
        case STATE_RECORDING:
            printf("recording");
            break;
        default:
            printf("unknown state");
            break;
    }
    
    printf("\n");
    
    return G_SOURCE_CONTINUE;
}

static void on_record_button_toggle(GtkWidget *button, gpointer user_data) {
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == TRUE) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appState.playButton), FALSE);
        appState.state = STATE_RECORDING;
    } else {
        appState.state = STATE_IDLE;
    }
    
    
}

static void on_play_button_toggle(GtkWidget *button, gpointer user_data) {
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == TRUE) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appState.recordButton), FALSE);
        appState.state = STATE_PLAYING;
    } else {
        appState.state = STATE_IDLE;
    }
    
    
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkBuilder *builder = gtk_builder_new_from_file("ui/linuxmacro.ui");
    
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "app_window"));
    gtk_window_set_application(GTK_WINDOW(window), app);
    
    GtkWidget *baseGrid = GTK_WIDGET(gtk_builder_get_object(builder, "base_grid"));
    
    appState.recordButton = GTK_WIDGET(gtk_builder_get_object(builder, "record_toggle_button"));
    appState.playButton = GTK_WIDGET(gtk_builder_get_object(builder, "play_toggle_button"));
    appState.state = STATE_IDLE;
    
    g_signal_connect(appState.recordButton, "toggled", G_CALLBACK(on_record_button_toggle), NULL);
    g_signal_connect(appState.playButton, "toggled", G_CALLBACK(on_play_button_toggle), NULL);
    
    g_idle_add(main_app_tick, NULL);
    
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    
    GtkApplication *app = gtk_application_new("com.linuxMacro.allivka.github", G_APPLICATION_DEFAULT_FLAGS);
    
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    g_object_unref(app);
    
    return status;
}