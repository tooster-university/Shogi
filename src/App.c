//
// Created by Tooster on 22.01.2018.
//

#include "App.h"
#include <gtk/gtk.h>
#include <stdlib.h>
#include "Utils.h"
#include "ResourceManager.h"
#include "Logger.h"
#include "Model.h"


double SHOGI_SCALE_FACTOR;
static enum SHOGI_STATE app_state = SHOGI_STATE_NEW_GAME_MENU;
static ShogiModel *model;
static GtkWidget *window;
static GtkWidget *board;
static GtkWidget *white_timer;
static GtkWidget *black_timer;
static GtkWidget *footer;
static GtkWidget *footer_player_label;
static cairo_surface_t *surface = NULL; // surface to store board
static GtkWidget *hand_labels[2][SHOGI_PAWN_COUNT]; // 0 for white, 1 for black...
static GtkWidget *hand_buttons[2][SHOGI_PAWN_COUNT]; // ...same

/// Macros returning clicked square on board as on image
#define SHOGI_TO_BOARD_COL(x) (((x)/SHOGI_SCALE_FACTOR < 68 || (x)/SHOGI_SCALE_FACTOR > 932) ?\
                                -1 : 10-((x)/SHOGI_SCALE_FACTOR-68)/96)

#define SHOGI_TO_BOARD_ROW(y) (((y)/SHOGI_SCALE_FACTOR < 68 || (y)/SHOGI_SCALE_FACTOR > 932) ?\
                                -1 : 1+((y)/SHOGI_SCALE_FACTOR-68)/96)
#define COLOR(c) ((c)/255.0)

static int shogi_init() {
    // set scale factor so the window will take approx 70% of the screen height
    SHOGI_SCALE_FACTOR =
            gdk_screen_get_height(gdk_display_get_default_screen(gdk_display_get_default())) * 0.7 / 1060.0;

    if (shogi_resource_manager_init() != 0)
        return 1;

    model = shogi_model_init();
    if (model == NULL)
        return 2;

    app_state = SHOGI_STATE_NEW_GAME_MENU;

    for (int i = 0; i < SHOGI_PAWN_COUNT; ++i) {
        hand_labels[0][i] = gtk_label_new(NULL);
        hand_labels[1][i] = gtk_label_new(NULL);
    }
    return 0;
}

static void close_window(void) {
    if (surface)
        cairo_surface_destroy(surface);
}


static void clear_board(void) {
    cairo_t *cr;

    cr = cairo_create(surface);

    cairo_scale(cr, SHOGI_SCALE_FACTOR, SHOGI_SCALE_FACTOR); // scale down graphics to fit screens
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    cairo_set_source_surface(cr, shogi_resource_manager_get_board(), 0, 0);
    cairo_paint(cr);

    enum SHOGI_PAWN_DETAILED **board = shogi_model_get_board();
    char **available_moves = shogi_model_get_available_moves();

    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {

            if (board[i][j] != SHOGI_PAWN_DETAILED_NONE) {
                cairo_set_source_surface(cr, shogi_resource_manager_get_pawn(board[i][j]), 68 + 96 * j, 68 + 96 * i);
                cairo_paint(cr);
            }

            if (available_moves[i][j] != ' ') {
                if (available_moves[i][j] == 'x')
                    cairo_set_source_rgba(cr, COLOR(66), COLOR(134), COLOR(244), 0.5);
                if (available_moves[i][j] == 'o')
                    cairo_set_source_rgba(cr, COLOR(150), COLOR(150), COLOR(150), 0.3);
                cairo_rectangle(cr, 68 + 96 * j, 68 + 96 * i, 96, 96);
                cairo_fill(cr);
            }
        }
    }
    // TODO: shogi_model_get_available_moves

    // Invoke on click
    /// gtk_widget_queue_draw_area(widget, x - 3, y - 3, 6, 6);

    cairo_destroy(cr);
}

static void ui_reload() {
    clear_board(); // first redraw to check for wins at print the king capture
    gtk_widget_queue_draw(board);

    enum SHOGI_MODEL_MODE model_mode = shogi_model_get_mode();
    if (model_mode == WHITE_WIN || model_mode == BLACK_WIN) {
        GtkWidget *dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(window),
                                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                               GTK_MESSAGE_INFO,
                                                               GTK_BUTTONS_CLOSE,
                                                               model_mode == WHITE_WIN ?
                                                               "<span weight='bold' font='15'>White wins.</span>" :
                                                               "<span weight='bold' font='15'>Black wins.</span>"
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    if (model_mode == PROMOTING) {
        GtkWidget *dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(window),
                                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                               GTK_MESSAGE_INFO,
                                                               GTK_BUTTONS_YES_NO,
                                                               "<span weight='bold' font='15'>Do you want to pomote this pawn?</span>"
        );

        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        if (response == GTK_RESPONSE_YES)
            shogi_model_promote(TRUE);
        else
            shogi_model_promote(FALSE);
    }

    clear_board(); // second redraw to print promotions
    gtk_widget_queue_draw(board);

    for (int i = SHOGI_PAWN_G; i < SHOGI_PAWN_COUNT; ++i) {
        gtk_widget_set_sensitive(hand_buttons[0][i], !shogi_model_is_black_turn());
        gtk_widget_set_sensitive(hand_buttons[1][i], shogi_model_is_black_turn());
    }

    // TODO change labels for hand pieces
    int *white_hand = shogi_model_get_hand(TRUE);
    int *black_hand = shogi_model_get_hand(FALSE);

    for (int j = 0; j < SHOGI_PAWN_COUNT; ++j) {
        char label_text[61];
        sprintf(label_text, "<span foreground='#231916' weight='bold' font='15'>%d</span>", white_hand[j]);
        gtk_label_set_label(GTK_LABEL(hand_labels[0][j]), label_text);
        sprintf(label_text, "<span foreground='#231916' weight='bold' font='15'>%d</span>", black_hand[j]);
        gtk_label_set_label(GTK_LABEL(hand_labels[1][j]), label_text);
    }
    gtk_label_set_label(GTK_LABEL(footer_player_label),
                        shogi_model_is_black_turn() ?
                        "<span weight='bold' foreground='#f9fad8' background='#231916' font='15'>          【 BLACK'S TURN 】          </span>"
                                                    :
                        "<span weight='bold' foreground='#231916' background='#f9fad8' font='15'>          【 WHITE'S TURN 】          </span>");


}

// Create board surface for rendering
static gboolean configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
    if (surface)
        cairo_surface_destroy(surface);

    surface = gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                                CAIRO_CONTENT_COLOR,
                                                gtk_widget_get_allocated_width(widget),
                                                gtk_widget_get_allocated_height(widget));

    /* Initialize the surface to white */
    clear_board();

    /* We've handled the configure event, no need for further processing. */
    return TRUE;
}

// Redraw gameboard
static gboolean clear_board_cb(GtkWidget *widget, cairo_t *cr, gpointer data) {
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);

    return FALSE;
}

// Handle gameboard events
static gboolean gameboard_press_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    /* paranoia check, in case we haven't gotten a configure event */
    if (surface == NULL)
        return FALSE;

    if (event->button == GDK_BUTTON_PRIMARY) {
        int col = (int) SHOGI_TO_BOARD_COL(event->x);
        int row = (int) SHOGI_TO_BOARD_ROW(event->y);
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Clicked coordinates [row=%d; col=%d]", row, col);
        if (shogi_model_click(col, row)) { // on any state change shogi_model_click returns true
            ui_reload();
        }
    }

    /* We've handled the event, stop processing */
    return TRUE;
}

//---------------------------------------------------------------------------------------------------------------------- TODO section
static void new_game_response(GtkWidget *w, gpointer data) { // FIXME
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Begin new game creator...");
    printf("TODO new_game\n");
    shogi_model_reset();
    ui_reload();
    app_state = SHOGI_STATE_NEW_GAME_MENU;
}

static void save_game_response(GtkWidget *w, gpointer data) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Begin saving game...");
    printf("TODO save_game\n");
}

static void load_game_response(GtkWidget *w, gpointer data) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Begin loading game...");
    printf("TODO load_game\n");
}

static void show_history_response(GtkWidget *w, gpointer data) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Showing history.");
    printf("TODO show_history\n");
}

static void export_history_response(GtkWidget *w, gpointer data) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Begin export history...");
    printf("TODO export_history\n");
}

static void rules_response(GtkWidget *w, gpointer data) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Launched rules dialog.");
    printf("TODO rules\n");
}

static void info_response(GtkWidget *w, gpointer data) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Launched info dialog.");
    printf("TODO info\n");
}
//----------------------------------------------------------------------------------------------------------------------

static void button_drop_response(GtkWidget *w, gpointer data) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Clicked drop button with ID:%d.", (int) (intptr_t) data);
    printf("clicked %d\n", (int) (intptr_t) data); // hack to pass integer as pointer address (intptr_t to silence warn)
    if (shogi_model_drop_mode((enum SHOGI_PAWN_DETAILED) ((int) (intptr_t) data)));
        // disable button

    ui_reload();

}

static GtkWidget *create_hand_box(gboolean is_white) {
    GtkWidget *side_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // TIMER
    GtkWidget *whiteTimerFrame = gtk_frame_new("white player's timer");
    white_timer = gtk_label_new("<span foreground='#231916' weight='bold' font='20'>00:00</span>");
    gtk_label_set_use_markup(GTK_LABEL(white_timer), TRUE);

    gtk_container_add(GTK_CONTAINER(whiteTimerFrame), white_timer);
    gtk_box_pack_start(GTK_BOX(side_panel), whiteTimerFrame, FALSE, TRUE, 10);

    gtk_box_pack_end(GTK_BOX(side_panel), gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), FALSE, FALSE,
                     (guint) (68 * SHOGI_SCALE_FACTOR / 2));  // separator from bottom

    // BUTTON BOXES
//    GtkWidget *button_grid = gtk_grid_new();
    GtkWidget *button_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *button_row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    for (int i = SHOGI_PAWN_G; i < SHOGI_PAWN_COUNT; ++i) {
        if (i % 2 == 0)
            button_row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);

        hand_buttons[is_white ? 0 : 1][i] = gtk_button_new();

        // ----------------
        // Setting texture for button
        GdkPixbuf *original = gdk_pixbuf_get_from_surface(
                shogi_resource_manager_get_pawn((enum SHOGI_PAWN_DETAILED) i * 2 + (is_white ? 0 : 1)),
                0, 0, 96, 96);
        GdkPixbuf *scaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
                                           (int) (96 * SHOGI_SCALE_FACTOR), (int) (96 * SHOGI_SCALE_FACTOR));
        gdk_pixbuf_scale(original, scaled, 0, 0, (int) (96 * SHOGI_SCALE_FACTOR), (int) (96 * SHOGI_SCALE_FACTOR),
                         0, 0,
                         SHOGI_SCALE_FACTOR, SHOGI_SCALE_FACTOR, GDK_INTERP_BILINEAR);
        gtk_button_set_image(GTK_BUTTON(hand_buttons[is_white ? 0 : 1][i]), gtk_image_new_from_pixbuf(scaled));
        // ----------------

        // small hack - pass pointer as NULL + ID so ID is hidden inside pointer address
        g_signal_connect(hand_buttons[is_white ? 0 : 1][i], "clicked", G_CALLBACK(button_drop_response),
                         NULL + (i * 2 + (is_white ? 0 : 1)));

        gtk_label_set_use_markup(GTK_LABEL(hand_labels[is_white ? 0 : 1][i]), TRUE);

        gtk_box_pack_start(GTK_BOX(button_row_box), hand_buttons[is_white ? 0 : 1][i], FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(button_row_box), hand_labels[is_white ? 0 : 1][i], FALSE, FALSE, 0);
        if (i % 2 == 0)
            gtk_box_pack_start(GTK_BOX(button_row_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE,
                               16);

        // buttons in groups of 2
        if (i % 2 == 1)
            gtk_box_pack_start(GTK_BOX(button_vbox), button_row_box, FALSE, FALSE, 0);

    }
    gtk_box_pack_end(GTK_BOX(side_panel), button_vbox, FALSE, TRUE, 0);
    return side_panel;
}

static void activate(GtkApplication *app, gpointer data) {
    GtkWidget *vbox;
    GtkWidget *menubar;
    GtkWidget *hbox;

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Starting shogi app...");

    if (shogi_init() != 0) // exit immediately if couldn't initialize
        abort();


    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW (window), "Shogi by Tooster");
    //gtk_window_set_resizable(GTK_WINDOW(window), FALSE); // disable resize - fixed size window for shogi

    g_signal_connect (window, "destroy", G_CALLBACK(close_window), NULL); // register handler for close operation

    // border from the edge of app window
    gtk_container_set_border_width(GTK_CONTAINER (window), 0);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); // main vertical box
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);



    //-----------------------------------------------------------
    // CREATING MENU BAR
    //-----------------------------------------------------------
    menubar = gtk_menu_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

    GtkWidget *main_menu = gtk_menu_new();
    GtkWidget *help_menu = gtk_menu_new();

    GtkWidget *menu_item;
    menu_item = gtk_menu_item_new_with_label("Menu");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), main_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menu_item);

    menu_item = gtk_menu_item_new_with_label("Help");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menu_item);

    // new
    menu_item = gtk_menu_item_new_with_label("New game...");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), menu_item);
    g_signal_connect(menu_item, "activate", G_CALLBACK(new_game_response), NULL);

    // load
    menu_item = gtk_menu_item_new_with_label("Save game");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), menu_item);
    g_signal_connect(menu_item, "activate", G_CALLBACK(save_game_response), NULL);

    //save
    menu_item = gtk_menu_item_new_with_label("Load game...");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), menu_item);
    g_signal_connect(menu_item, "activate", G_CALLBACK(load_game_response), NULL);

    menu_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), menu_item);

    // history show
    menu_item = gtk_menu_item_new_with_label("Show history");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), menu_item);
    g_signal_connect(menu_item, "activate", G_CALLBACK(show_history_response), NULL);

    // history export
    menu_item = gtk_menu_item_new_with_label("Export history");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), menu_item);
    g_signal_connect(menu_item, "activate", G_CALLBACK(export_history_response), NULL);

    // rules
    menu_item = gtk_menu_item_new_with_label("rules");
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), menu_item);
    g_signal_connect(menu_item, "activate", G_CALLBACK(rules_response), NULL);

    // info
    menu_item = gtk_menu_item_new_with_label("info");
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), menu_item);
    g_signal_connect(menu_item, "activate", G_CALLBACK(info_response), NULL);


    //-----------------------------------------------------------
    // MIDDLE AREA
    //-----------------------------------------------------------
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    //////
    /// create white hand box
    //////
    gtk_box_pack_start(GTK_BOX(hbox), create_hand_box(TRUE), FALSE, FALSE, 8);

    //////
    /// create board box
    //////
    GtkWidget *gameboard_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME (gameboard_frame), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(hbox), gameboard_frame, FALSE, TRUE, 8);

    board = gtk_drawing_area_new();
    /* set a minimum size */
    gtk_widget_set_size_request(board, (gint) (1000 * SHOGI_SCALE_FACTOR), (gint) (1000 * SHOGI_SCALE_FACTOR));

    gtk_container_add(GTK_CONTAINER(gameboard_frame), board);

    /* Signals used to handle the backing surface */
    g_signal_connect (board, "draw",
                      G_CALLBACK(clear_board_cb), NULL);
    g_signal_connect (board, "configure-event",
                      G_CALLBACK(configure_event_cb), NULL);

    /* Event signals */
    g_signal_connect (board, "button-press-event",
                      G_CALLBACK(gameboard_press_event_cb), NULL);

    /* Ask to receive events the drawing area doesn't normally
     * subscribe to. In particular, we need to ask for the
     * button press that want to handle.
     */
    gtk_widget_set_events(board, gtk_widget_get_events(board)
                                 | GDK_BUTTON_PRESS_MASK);



    //////
    /// create black hand box
    //////

    gtk_box_pack_start(GTK_BOX(hbox), create_hand_box(FALSE), FALSE, FALSE, 8);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    //-----------------------------------------------------------
    // FOOTER
    //-----------------------------------------------------------

    footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 32);
    footer_player_label = gtk_label_new(NULL);
    gtk_label_set_use_markup(GTK_LABEL(footer_player_label), TRUE);
    gtk_box_pack_start(GTK_BOX(footer), footer_player_label, TRUE, TRUE, 0);


    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), footer, FALSE, TRUE, 0);

    //-----------------------------------------------------------
    ui_reload();
    gtk_widget_show_all(window);

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_INFO, "Shogi app started.");
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