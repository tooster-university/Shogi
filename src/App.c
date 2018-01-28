//
// Created by Tooster on 22.01.2018.
//

#include <gtk/gtk.h>
#include <stdlib.h>
#include <math.h>
#include "ResourceManager.h"
#include "Logger.h"
#include "Model.h"

/* Surface to store current scribbles */
static cairo_surface_t *surface = NULL;


// TODO: push to GuiController
static void clear_surface(void) {
    cairo_t *cr;

    cr = cairo_create(surface);

    cairo_scale(cr, SHOGI_SCALE_FACTOR, SHOGI_SCALE_FACTOR); // scale down graphics to fit screens
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    cairo_set_source_surface(cr, shogi_resource_manager_get_board(), 0, 0);
    cairo_paint(cr);
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i * 4 + j < SHOGI_PAWN_DETAILED_TYPE_COUNT) {
                cairo_set_source_surface(cr, shogi_resource_manager_get_pawn(i * 4 + j), 68 + 96 * i, 68 + 96 * j);
                cairo_paint(cr);
            }
        }
    }

    // Invoke on click
    /// gtk_widget_queue_draw_area(widget, x - 3, y - 3, 6, 6);

    cairo_destroy(cr);
}

// TODO: push to GuiController
/* Create a new surface of the appropriate size to store our scribbles */
static gboolean configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
    if (surface)
        cairo_surface_destroy(surface);

    surface = gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                                CAIRO_CONTENT_COLOR,
                                                gtk_widget_get_allocated_width(widget),
                                                gtk_widget_get_allocated_height(widget));

    /* Initialize the surface to white */
    clear_surface();

    /* We've handled the configure event, no need for further processing. */
    return TRUE;
}

// TODO: push to GuiController
/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data) {
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);

    return FALSE;
}

// TODO: push to GuiController
// Handle button press events.
static gboolean button_press_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    /* paranoia check, in case we haven't gotten a configure event */
    if (surface == NULL)
        return FALSE;

    if (event->button == GDK_BUTTON_PRIMARY) {
        //shogi_model_click(widget, toBoard(event->x, event->y));
        clear_surface();
        gtk_widget_queue_draw(widget);
    }

    /* We've handled the event, stop processing */
    return TRUE;
}


static void close_window(void) {
    if (surface)
        cairo_surface_destroy(surface);
}

int shogi_init() {
    if (shogi_resource_manager_init() != 0)
        return 1;

    return 0;
}

int shogi_close() {
    return 0;
}

static void activate(GtkApplication *app, gpointer user_data) {

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Starting shogi app...");

    if (shogi_init() != 0) // exit immediately if couldn't initialize
        abort();

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_INFO, "Shogi app started.");
    GtkWidget *window;
    GtkWidget *frame;
    GtkWidget *drawing_area;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW (window), "Shogi by Tooster");

    g_signal_connect (window, "destroy", G_CALLBACK(close_window), NULL);

    gtk_container_set_border_width(GTK_CONTAINER (window), 8);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER (window), frame);

    drawing_area = gtk_drawing_area_new();
    /* set a minimum size */
    gtk_widget_set_size_request(drawing_area, 1000 * SHOGI_SCALE_FACTOR, 1000 * SHOGI_SCALE_FACTOR);

    gtk_container_add(GTK_CONTAINER (frame), drawing_area);

    /* Signals used to handle the backing surface */
    g_signal_connect (drawing_area, "draw",
                      G_CALLBACK(draw_cb), NULL);
    g_signal_connect (drawing_area, "configure-event",
                      G_CALLBACK(configure_event_cb), NULL);

    /* Event signals */
    g_signal_connect (drawing_area, "button-press-event",
                      G_CALLBACK(button_press_event_cb), NULL);

    /* Ask to receive events the drawing area doesn't normally
     * subscribe to. In particular, we need to ask for the
     * button press that want to handle.
     */
    gtk_widget_set_events(drawing_area, gtk_widget_get_events(drawing_area)
                                        | GDK_BUTTON_PRESS_MASK);

    gtk_widget_show_all(window);

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Closing Shogi app...");
    if (shogi_close() != 0)
        abort();
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("ttr.Shogi", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION (app), argc, argv);
    g_object_unref(app);

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_INFO, "App closed with status %d.", status);
    shogi_logger_close();

    return status;
}