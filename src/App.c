//
// Created by Tooster on 22.01.2018.
//

#include <stdint.h>
#include <string.h>
#include "App.h"
#include "ResourceManager.h"
#include "Logger.h"


double SHOGI_SCALE_FACTOR;
static ShogiModel *model;
static GtkWidget *window;
static GtkWidget *board;
static GtkWidget *footer;
static GtkWidget *footer_player_label;
static cairo_surface_t *surface = NULL; // surface to store board
static GtkWidget *timer[2];
static GtkWidget *hand_labels[2][SHOGI_PAWN_COUNT]; // 0 for white, 1 for black...
static GtkWidget *hand_buttons[2][SHOGI_PAWN_COUNT]; // ...same
static GtkWidget *resign_button[2];
static gboolean TIMER_RUN = FALSE;
static clock_t t0 = 0;

/// Macros returning clicked square on board as on image
#define SHOGI_TO_BOARD_COL(x) (((x)/SHOGI_SCALE_FACTOR < 68 || (x)/SHOGI_SCALE_FACTOR > 932) ?\
                                -1 : 10-((x)/SHOGI_SCALE_FACTOR-68)/96)

#define SHOGI_TO_BOARD_ROW(y) (((y)/SHOGI_SCALE_FACTOR < 68 || (y)/SHOGI_SCALE_FACTOR > 932) ?\
                                -1 : 1+((y)/SHOGI_SCALE_FACTOR-68)/96)
#define COLOR(c) ((c)/255.0)

static int shogi_init() {
    // set scale factor so the window will take approx 70% of the screen height
    SHOGI_SCALE_FACTOR =
            gdk_screen_get_height(gdk_display_get_default_screen(gdk_display_get_default())) * SHOGI_UI_SCALE / 1060.0;

    if (shogi_resource_manager_init() != 0)
        return 1;

    model = shogi_model_init();
    if (model == NULL)
        return 2;


    g_timeout_add(15, (GSourceFunc) timer_cb, NULL);

    for (int i = 0; i < SHOGI_PAWN_COUNT; ++i) {
        hand_labels[0][i] = gtk_label_new(NULL);
        hand_labels[1][i] = gtk_label_new(NULL);
    }

    timer[0] = gtk_label_new("<span foreground='#231916' weight='bold' font='20'>00:00</span>");
    timer[1] = gtk_label_new("<span foreground='#231916' weight='bold' font='20'>00:00</span>");
    gtk_label_set_use_markup(GTK_LABEL(timer[0]), TRUE);
    gtk_label_set_use_markup(GTK_LABEL(timer[1]), TRUE);
    resign_button[0] = gtk_button_new_with_label("Resign");
    resign_button[1] = gtk_button_new_with_label("Resign");

    return 0;
}

static void close_window(void) {
    if (surface)
        cairo_surface_destroy(surface);
    TIMER_RUN = FALSE; // after close the timeout still runs for a while, thus labels are improper
    shogi_model_close();
}

static void redraw_board(void) {
    cairo_t *cr;

    cr = cairo_create(surface);

    cairo_scale(cr, SHOGI_SCALE_FACTOR, SHOGI_SCALE_FACTOR); // scale down graphics to fit screens
    cairo_set_source_rgba(cr, 254, 194, 72, 1);
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

    // Invoke on click
    /// gtk_widget_queue_draw_area(widget, x - 3, y - 3, 6, 6);

    cairo_destroy(cr);
}

static void ui_reload() {
    redraw_board(); // first redraw to check for wins at print the king capture
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

    redraw_board(); // second redraw to print promotions
    gtk_widget_queue_draw(board);

    char timer_label[65];
    guint32 time_left = (guint32) shogi_model_timer_get_time(TRUE);
    sprintf(timer_label, "<span foreground='#231916' weight='bold' font='20'>%02d:%02d</span>", TO_MINUTES(time_left),
            TO_SECONDS(time_left));
    gtk_label_set_label(GTK_LABEL(timer[0]), timer_label);

    time_left = (guint32) shogi_model_timer_get_time(FALSE);
    sprintf(timer_label, "<span foreground='#231916' weight='bold' font='20'>%02d:%02d</span>", TO_MINUTES(time_left),
            TO_SECONDS(time_left));
    gtk_label_set_label(GTK_LABEL(timer[1]), timer_label);

    gtk_widget_queue_draw(timer[0]);
    gtk_widget_queue_draw(timer[1]);

    enum SHOGI_MODEL_MODE mm = shogi_model_get_mode();
    gboolean any_won = (mm == WHITE_WIN || mm == BLACK_WIN);
    int *white_hand = shogi_model_get_hand(TRUE);
    int *black_hand = shogi_model_get_hand(FALSE);
    for (int i = SHOGI_PAWN_G; i < SHOGI_PAWN_COUNT; ++i) {
        gtk_widget_set_sensitive(hand_buttons[0][i],
                                 any_won ? FALSE : (!shogi_model_is_black_turn() && white_hand[i] > 0));
        gtk_widget_set_sensitive(hand_buttons[1][i],
                                 any_won ? FALSE : (shogi_model_is_black_turn() && black_hand[i] > 0));
    }

    gtk_widget_set_sensitive(resign_button[0], any_won ? FALSE : !shogi_model_is_black_turn());
    gtk_widget_set_sensitive(resign_button[1], any_won ? FALSE : shogi_model_is_black_turn());


    for (int j = 0; j < SHOGI_PAWN_COUNT; ++j) {
        char label_text[62];
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
static gboolean configure_event_cb(GtkWidget *widget) {
    if (surface)
        cairo_surface_destroy(surface);

    surface = gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                                CAIRO_CONTENT_COLOR,
                                                gtk_widget_get_allocated_width(widget),
                                                gtk_widget_get_allocated_height(widget));

    /* Initialize the surface to white */
    redraw_board();

    gint w;
    gint h;
    gtk_window_get_default_size(GTK_WINDOW(window), &w, &h);
    if (w != -1 && h != -1)
        gtk_window_resize(GTK_WINDOW(window), w, h);

    /* We've handled the configure event, no need for further processing. */
    return TRUE;
}

// Redraw gameboard
static gboolean redraw_board_cb(GtkWidget *widget, cairo_t *cr, gpointer data) {
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

static gboolean timer_cb(gpointer data) {

    if (!TIMER_RUN) {
        t0 = clock();
        return TRUE;
    }
    if (t0 == 0)
        t0 = clock();
    clock_t t1 = clock();

    char timer_label[65];

    guint32 time_left = (guint32) shogi_model_timer_get_time(TRUE);
    sprintf(timer_label, "<span foreground='#231916' weight='bold' font='20'>%02d:%02d</span>", TO_MINUTES(time_left),
            TO_SECONDS(time_left));
    gtk_label_set_label(GTK_LABEL(timer[0]), timer_label);

    time_left = (guint32) shogi_model_timer_get_time(FALSE);
    sprintf(timer_label, "<span foreground='#231916' weight='bold' font='20'>%02d:%02d</span>", TO_MINUTES(time_left),
            TO_SECONDS(time_left));
    gtk_label_set_label(GTK_LABEL(timer[1]), timer_label);

    gtk_widget_queue_draw(timer[0]);
    gtk_widget_queue_draw(timer[1]);

    if (shogi_model_get_mode() == WHITE_WIN || shogi_model_get_mode() == BLACK_WIN) {
        TIMER_RUN = FALSE;
        ui_reload();
    }

    shogi_model_timer_decrease((t1 - t0) * 1000 / CLOCKS_PER_SEC); // get actual time difference

    t0 = t1;
    return TRUE;
}

static void resign_cb() {
    shogi_model_resign();
    TIMER_RUN = FALSE;
    ui_reload();
}

//----------------------------------------------------------------------------------------------------------------------
static void new_game_response(GtkWidget *w, gpointer data) { // FIXME
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Begin new game creator...");

    GtkWidget *dialog = gtk_dialog_new_with_buttons("New game options.",
                                                    GTK_WINDOW(window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "Start", GTK_RESPONSE_OK,
                                                    "Cancel", GTK_RESPONSE_CANCEL,
                                                    NULL);
    GtkWidget *context_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *frame = gtk_frame_new("Timer settings");
    GtkWidget *grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(frame), 3);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 4);


    gtk_box_pack_start(GTK_BOX(context_area), frame, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), grid);

    GtkWidget *label = gtk_label_new("minutes");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);
    label = gtk_label_new("seconds");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);
    GtkWidget *minutes_input = gtk_spin_button_new_with_range(0, 999, 1);
    GtkWidget *seconds_input = gtk_spin_button_new_with_range(0, 59, 1);

    gtk_grid_attach(GTK_GRID(grid), minutes_input, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), seconds_input, 1, 2, 1, 1);

    label = gtk_label_new(" Set to 00:00 to play without time restrictions. ");
    gtk_widget_set_sensitive(label, FALSE);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 3, 1);

    gtk_widget_show_all(frame);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK) {
        shogi_model_reset();
        guint32 minutes = (guint32) gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(minutes_input));
        guint32 seconds = (guint32) gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(seconds_input));
        shogi_model_timer_set(minutes * 60 * 1000 + seconds * 1000);
        ui_reload();
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_INFO, "New game started");
        TIMER_RUN = TRUE;
    }

    gtk_widget_destroy(dialog);
}

static void save_game_response(GtkWidget *w, gpointer data) {
    if (shogi_model_get_mode() == WHITE_WIN || shogi_model_get_mode() == BLACK_WIN) // if either one won, don't save
        return;

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Saving game...");

    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save game", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "Cancel", GTK_RESPONSE_CANCEL,
                                                    "Save", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "save.shogi");

    gint result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_ACCEPT) {
        char *filename;
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        FILE *file = fopen(filename, "wb+"); // opens new binary file for save

        // save structure as follows:
        // [state][black_turn][timed][timer[2]][entries][{history}]
        char *state = shogi_model_serialize_state();
        fwrite(state, sizeof(char), SHOGI_MODEL_SERIALIZED_STATE_LENGTH, file); // copy state into file
        free(state);
        gboolean is_black_turn = shogi_model_is_black_turn();
        fwrite(&is_black_turn, sizeof(gboolean), 1, file); // if black turn
        fwrite(&model->TIMED_MODE, sizeof(gboolean), 1, file); // if timed mode
        fwrite(&model->timer, sizeof(gint64), 2, file); // timers time

        fwrite(&model->history_entries, sizeof(int), 1, file); // entries in history
        fseek(model->history, 0, SEEK_SET); // rewind history file to the beginning
        for (int i = 0; i < model->history_entries; ++i) { // copy history
            ShogiModelHistoryEntry entry;
            fread(&entry, sizeof(ShogiModelHistoryEntry), 1, model->history);
            fwrite(&entry, sizeof(ShogiModelHistoryEntry), 1, file);
        }
        fseek(model->history, 0, SEEK_END); // rewind history file to the end

        fflush(file);
        fclose(file);
    }

    gtk_widget_destroy(dialog);
}

static void load_game_response(GtkWidget *w, gpointer data) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Begin loading game...");
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save game", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "Cancel", GTK_RESPONSE_CANCEL,
                                                    "Open", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_ACCEPT) {
        char *filename;
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        FILE *file = fopen(filename, "r"); // opens new binary file for save

        fseek(file, 0, SEEK_SET);
        shogi_model_load_game(file);
        if (model->TIMED_MODE) TIMER_RUN = TRUE;

        fflush(file);
        fclose(file);
    }

    gtk_widget_destroy(dialog);
    ui_reload();
}

static void show_history_response(GtkWidget *w, gpointer data) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Showing history.");
    GtkWidget *dialog = gtk_dialog_new_with_buttons("History",
                                                    GTK_WINDOW(window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "Close", GTK_RESPONSE_CLOSE,
                                                    NULL);
    GtkWidget *context_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 3);
    gtk_box_pack_start(GTK_BOX(context_area), frame, TRUE, TRUE, 0);
    gtk_widget_set_size_request(frame, 200, 300);

    GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);
    GtkWidget *view = gtk_text_view_new_with_buffer(buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW (view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW (view), GTK_WRAP_WORD);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW (view), 3);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW (view), FALSE);

    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (sw),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER (sw), view);
    gtk_container_add(GTK_CONTAINER(frame), sw);
    gtk_widget_show_all(sw);

    gtk_widget_show_all(frame);

    // copy entries from binary to buffer
    fseek(model->history, 0, SEEK_SET);
    ShogiModelHistoryEntry entry;
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(buffer), &iter);
    for (int i = 0; i < model->history_entries; ++i) {
        gchar str[30];
        if (i % 2 == 0) {
            sprintf(str, "%d.", i / 2 + 1);
            while (strlen(str) < 4) strcat(str, " ");
            gtk_text_buffer_insert(GTK_TEXT_BUFFER(buffer), &iter, g_strdup(str), -1);
        }
        fread(&entry, sizeof(ShogiModelHistoryEntry), 1, model->history);
        sprintf(str, "\t%s", entry.move);
        while (strlen(str) < 7) strcat(str, " ");
        gtk_text_buffer_insert(GTK_TEXT_BUFFER(buffer), &iter, g_strdup(str), -1);
        if (i % 2 == 1) {
            sprintf(str, "\n");
            gtk_text_buffer_insert(GTK_TEXT_BUFFER(buffer), &iter, g_strdup(str), -1);
        }
    }

    gtk_dialog_run(GTK_DIALOG(dialog));
    fseek(model->history, 0, SEEK_END);
    gtk_widget_destroy(dialog);
}

static void info_response(GtkWidget *w, gpointer data) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Launched info dialog.");
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Info",
                                                    GTK_WINDOW(window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    GTK_BUTTONS_NONE);
    GtkWidget *context_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(context_area), frame, FALSE, TRUE, 0);
    gtk_frame_set_shadow_type(GTK_FRAME (frame), GTK_SHADOW_NONE);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 30);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);


    GtkWidget *label = gtk_label_new("<span weight='bold'>Shogi app v1.0</span>\n"
                                             "by Tooster\n"
                                             "\n");
    gtk_label_set_use_markup(GTK_LABEL (label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
    label = gtk_label_new("Shogi, known also as Japanese chess is a board strategy game for two players.\n"
                                  "To see all the rules see <a href='https://en.wikipedia.org/wiki/Shogi'>this wikipedia page</a>\n"
                                  "\n"
                                  "The app supports two game modes: with limited time per player or with unlimited time.\n"
                                  "Short version of rules:\n"
                                  " * The main objective is to capture opponents king (game ends on capture, not checkmate).\n"
                                  " * Shogi pieces capture the same way they move, also there is no castling move in shogi.\n"
                                  " * Captured pieces are retained in hand and can be later placed on the board (drops). Pieces\n"
                                  "   in hand aren't promoted.\n"
                                  " * Player may either move, or drop captured piece on board.\n"
                                  " * Piece cannot capture on drop.\n"
                                  " * Dropped piece is always unpromoted.\n"
                                  " * A pawn, knight, or lance may not be dropped on the rows in a way they would have no legal \n"
                                  "   moves on subsequent turns (so black pawn and lance in row 1 and black knight in rows 1 and 2)\n"
                                  " * Pawn cannot be dropped to checkmate an opposing king, also it cannot be dropped in a column\n"
                                  "   with other unpromoted pawns.\n"
                                  " * Three furthest rows are the promotion zone. If a part of piece's path is inside opponent's\n"
                                  "   promotion zone, this piece may promote. All pieces except King ,Golden general and already\n"
                                  "   promoted pieces can promote. Promotion occurs only after the movement. Piece can promote\n"
                                  "   after capture. Pieces that couldn't possibly move on subsequent moves (for example the pawn in\n"
                                  "   the last row) must be promoted.\n"
                                  " * Promoted Pawn, Lance, Knight and Silver general move the same way as Golden general.\n"
                                  " * Promoted Rook and Bishop, except standard move pattern, can move in a 3x3 square around them.\n"
                                  " * Black moves first.\n"
                                  "\n"
                                  "Move patterns from black's perspective:\n"
                                  "<span font_family='monospace'>"
                                  "+-----------------------------------------------------------------------------------------------------+\n"
                                  "| letter indicates a piece, # a field it can jump to, lines, well, the line of movement               |\n"
                                  "|   [K]:King [G]:Golden general [S]:Silver general [N]:Knight [L]:Lance [B]:Bishop [R]:Rook [P]:Pawn  |\n"
                                  "|   [H]:Horse (promoted Bishop) [D]:Dragon( promoted Rook)    [O]: Promoted S, N, L, P                |\n"
                                  "+-----------------------------------------------------------------------------------------------------+\n"
                                  "|<span weight='bold'>    ###    ###    ###    # #     |     \\ /     |      #     \\#/    #|#    ###                        </span>|\n"
                                  "|<span weight='bold'>    #K#    #G#     S             L      B     -R-     P     #B#    -R-    #O#                        </span>|\n"
                                  "|<span weight='bold'>    ###     #     # #     N            / \\     |            /#\\    #|#      #                        </span>|\n"
                                  "+-----------------------------------------------------------------------------------------------------+\n"
                                  "</span>\n\n"
                                  "Moves history are saved in western notation format (move number) (black move) (white move)\n"
                                  "A single move is described like in <a href='https://en.wikipedia.org/wiki/Shogi_notation#Western_notation'>western notation</a>\n"
                                  "[PIECE][ORIGIN][MOVEMENT][DESTINATION][PROMOTION], where:\n"
                                  " - Piece is one of P,L,N,S,G,B,R,K with preceding + if it is promoted.\n"
                                  " - Origin is in format [column][row] and is omitted <span weight='bold'>only</span> on drop.\n"
                                  " - Movement is one of '-':move 'x':capture '*':drop\n"
                                  " - Destination similar to origin\n"
                                  " - Promotion is one of '+':if piece has been promoted, '=':if promotion was declined, ommited if no promotion was available.\n"
                                  "\n"
                                  "Only games in progress can be saved, because there is no sense in saving a won game."
    );
    gtk_label_set_use_markup(GTK_LABEL (label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_FILL);
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (sw),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER (sw), label);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
    gtk_widget_set_size_request(sw, -1, 400);
    gtk_widget_show_all(frame);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
//----------------------------------------------------------------------------------------------------------------------

static void button_drop_response(GtkWidget *w, gpointer data) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Clicked drop button with ID:%d.", (int) (intptr_t) data);
    // hack to pass integer as pointer address (intptr_t to silence warn)
    if (shogi_model_drop_mode((enum SHOGI_PAWN_DETAILED) ((int) (intptr_t) data)));
    // disable button

    ui_reload();

}

static GtkWidget *create_hand_box(gboolean is_white) {
    GtkWidget *side_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // TIMER
    int player_index = is_white ? 0 : 1;
    GtkWidget *timerFrame = gtk_frame_new(is_white ? "white player's timer" : "black player's timer");

    gtk_container_add(GTK_CONTAINER(timerFrame), timer[player_index]);
    gtk_box_pack_start(GTK_BOX(side_panel), timerFrame, FALSE, TRUE, 10);
    g_signal_connect(resign_button[player_index], "clicked", G_CALLBACK(resign_cb), NULL);
    gtk_box_pack_start(GTK_BOX(side_panel), resign_button[player_index], FALSE, TRUE, 0);

    gtk_box_pack_end(GTK_BOX(side_panel), gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), FALSE, FALSE,
                     (guint) (68 * SHOGI_SCALE_FACTOR / 2));  // separator from bottom

    // BUTTON BOXES
//    GtkWidget *button_grid = gtk_grid_new();
    GtkWidget *button_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *button_row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    for (int i = SHOGI_PAWN_P; i >= SHOGI_PAWN_G; --i) {
        if (i % 2 == 0)
            button_row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);

        hand_buttons[player_index][i] = gtk_button_new();

        // ----------------
        // Setting texture for button
        GdkPixbuf *original = gdk_pixbuf_get_from_surface(
                shogi_resource_manager_get_pawn((enum SHOGI_PAWN_DETAILED) i * 2 + (player_index)),
                0, 0, 96, 96);
        GdkPixbuf *scaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
                                           (int) (96 * SHOGI_SCALE_FACTOR), (int) (96 * SHOGI_SCALE_FACTOR));
        gdk_pixbuf_scale(original, scaled, 0, 0, (int) (96 * SHOGI_SCALE_FACTOR), (int) (96 * SHOGI_SCALE_FACTOR),
                         0, 0,
                         SHOGI_SCALE_FACTOR, SHOGI_SCALE_FACTOR, GDK_INTERP_BILINEAR);
        gtk_button_set_image(GTK_BUTTON(hand_buttons[player_index][i]), gtk_image_new_from_pixbuf(scaled));
        // ----------------

        // small hack - pass pointer as NULL + ID so ID is hidden inside pointer address
        g_signal_connect(hand_buttons[player_index][i], "clicked", G_CALLBACK(button_drop_response),
                         NULL + (i * 2 + (player_index)));

        gtk_label_set_use_markup(GTK_LABEL(hand_labels[player_index][i]), TRUE);

        gtk_box_pack_start(GTK_BOX(button_row_box), hand_buttons[player_index][i], FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(button_row_box), hand_labels[player_index][i], FALSE, FALSE, 0);
        gtk_widget_set_size_request(hand_labels[player_index][i], 30, 15);

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
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);


    //g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);
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
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

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
    menu_item = gtk_menu_item_new_with_label("History");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), menu_item);
    g_signal_connect(menu_item, "activate", G_CALLBACK(show_history_response), NULL);

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
    gtk_box_pack_start(GTK_BOX(hbox), create_hand_box(TRUE), FALSE, TRUE, 8);

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
                      G_CALLBACK(redraw_board_cb), NULL);
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

    gtk_box_pack_start(GTK_BOX(hbox), create_hand_box(FALSE), FALSE, TRUE, 8);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
    //-----------------------------------------------------------
    // FOOTER
    //-----------------------------------------------------------

    footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 32);
    footer_player_label = gtk_label_new(NULL);
    GtkWidget *footer_label_frame = gtk_frame_new(NULL);
    gtk_label_set_use_markup(GTK_LABEL(footer_player_label), TRUE);
    gtk_container_add(GTK_CONTAINER(footer_label_frame), footer_player_label);
    gtk_box_pack_start(GTK_BOX(footer), footer_label_frame, TRUE, FALSE, 0);


    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), footer, FALSE, TRUE, 0);

    //-----------------------------------------------------------
    ui_reload();
    gtk_widget_show_all(window);

    gint w;
    gint h;
    gtk_window_get_size(GTK_WINDOW(window), &w, &h);
    gtk_window_set_default_size(GTK_WINDOW(window), w, h);

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