//
// Created by Tooster on 22.01.2018.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "Model.h"
#include "Logger.h"

enum SHOGI_MODEL_MODE mode = NONE;
enum SHOGI_PAWN_DETAILED selected_pawn = SHOGI_PAWN_DETAILED_NONE;
int selected_col = 0;
int selected_row = 0;
// following needed for proper history append while promoting
int previous_col = 0;
int previous_row = 0;
gboolean previous_captured = false;


static gboolean is_black_turn = TRUE;
static ShogiModel *model;
// @formatter:off
// available moves pattern, overwritten in calculating hitmap
static char **available_moves;

// pawn move patterns are denoted by 3x3 matrices where:
// 'x' is a pawn
// 'o' is the field is field the pawn can go to
// '-' '|' '/' '\' are lines of available moves (accordingly: horizontal, vertical, diagonals)
// ' ' is square it cannot move
static char pawn_move_pattern[SHOGI_PAWN_DETAILED_COUNT / 2][3][3] = {
        {       /// K - KING
                {'o','o','o'},
                {'o','x','o'},
                {'o','o','o'}
        },
        {       /// G - GOLDEN
                {'o','o','o'},
                {'o','x','o'},
                {' ','o',' '}
        },
        {       /// S - SILVER
                {'o','o','o'},
                {' ','x',' '},
                {'o',' ','o'}
        },
        {       /// N - KNIGHT
                {'o',' ','o'},
                {' ',' ',' '},
                {' ','x',' '}
        },
        {       /// L - LANCE
                {' ','|',' '},
                {' ','x',' '},
                {' ',' ',' '}
        },
        {       /// B - BISHOP
                {'\\',' ','/'},
                {' ', 'x',' '},
                {'/', ' ','\\'}
        },
        {       /// R - ROOOK
                {' ','|',' '},
                {'-','x','-'},
                {' ','|',' '}
        },
        {       /// P - PAWN
                {' ','o',' '},
                {' ','x',' '},
                {' ',' ',' '}
        },
        {       /// Z - SILVER PROMOTED
                {'o','o','o'},
                {'o','x','o'},
                {' ','o',' '}
        },
        {       /// M - KNIGHT PROMOTED
                {'o','o','o'},
                {'o','x','o'},
                {' ','o',' '}
        },
        {       /// J - LANCE PROMOTED
                {'o','o','o'},
                {'o','x','o'},
                {' ','o',' '}
        },
        {       /// H - BISHOP PROMOTED
                {'\\','o','/'},
                {'o', 'x','o'},
                {'/', 'o','\\'}
        },
        {       /// D - ROOK PROMOTED
                {'o','|','o'},
                {'-','x','-'},
                {'o','|','o'}
        },
        {       /// O - PAWN PROMOTED
                {'o','o','o'},
                {'o','x','o'},
                {' ','o',' '}
        },
};
static char pawn_base_character[SHOGI_PAWN_COUNT] = {'K', 'G', 'S', 'N', 'L', 'B', 'R', 'P'};
// @formatter:on

// macros require absolute indexes in matrix (0..8 left to right, can be transformed from ROW(x) and COL(x))
/// check if coordinates are on board - [1..9]
#define SHOGI_MODEL_CORDS_GUT(col_ix, row_ix) ( (row_ix) >= 0 && (row_ix) < 9 && (col_ix) >= 0 && (col_ix) < 9 )
/// check if mine pawn occupies given place
#define SHOGI_MODEL_MINE_OCCUPIES(board, col_ix, row_ix, for_black) (((for_black) && SHOGI_PAWN_IS_BLACK(board [row_ix][col_ix])) || \
                                            (!(for_black) && SHOGI_PAWN_IS_WHITE(board [row_ix][col_ix])))
/// analogously to ^
#define SHOGI_MODEL_OTHER_OCCUPIES(board, col_ix, row_ix, for_black) (((for_black) && SHOGI_PAWN_IS_WHITE(board [row_ix][col_ix])) || \
                                            (!(for_black) && SHOGI_PAWN_IS_BLACK(board [row_ix][col_ix])))
/// checks if place on board is free
#define SHOGI_MODEL_IS_FREE(board, col_ix, row_ix) (board[row_ix][col_ix] == SHOGI_PAWN_NONE)

/// deallocates memory for 9x9 matrix
#define SHOGI_MODEL_FREE_MATRIX(matrix) do { for (int i = 0; i < 9; ++i) free(matrix[i]); free(matrix); }while(0)

/// Following macros are used during serialization and hashing
/// macros translating pawn to and from codes
#define SHOGI_MODEL_TO_PAWN_CODE(pawn_detailed) ((pawn_detailed) == SHOGI_PAWN_DETAILED_NONE ? (char) 32 : (char) (33 + (pawn_detailed)))
#define SHOGI_MODEL_FROM_PAWN_CODE(pawn_code) ((pawn_code) == 32 ? SHOGI_PAWN_DETAILED_NONE : (enum SHOGI_PAWN_DETAILED) ((pawn_code) - 33))
/// translates numbers int char codes
#define SHOGI_MODEL_TO_COUNT_CODE(count) ((char)((count)+32));
#define SHOGI_MODEL_FROM_COUNT_CODE(count_code) ((int)((count_code)-32));

//----------------------------------------------------------------------------------------------------------------------


ShogiModel *shogi_model_init() {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Creating new model...");
    model = malloc(sizeof(ShogiModel));
    if (model == NULL)
        return NULL;

    model->hand[0] = calloc(SHOGI_PAWN_COUNT, sizeof(int));
    model->hand[1] = calloc(SHOGI_PAWN_COUNT, sizeof(int));
    model->board = calloc(9, sizeof(enum SHOGI_PAWN_DETAILED *));
    if (model->hand[0] == NULL || model->hand[1] == NULL || model->board == NULL) {
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_FATAL, "One of model's fields couldn't be allocated.");
        free(model->hand[0]);
        free(model->hand[1]);
        free(model->board);
        free(model);
        return NULL;
    }

    for (int i = 0; i < 9; ++i) {
        model->board[i] = calloc(9, sizeof(enum SHOGI_PAWN_DETAILED));
        if (model->board[i] == NULL) {
            shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_FATAL, "Some rows in model couldn't be allocated.");
            while (i >= 0)
                free(model->board[i--]);
            free(model->hand[0]);
            free(model->hand[0]);
            free(model->board);
            free(model);
            return NULL;
        }
    }

    available_moves = calloc(9, sizeof(char *));
    for (int j = 0; j < 9; ++j)
        available_moves[j] = calloc(9, sizeof(char));

    shogi_model_hitmap_clear(available_moves);

    model->history = NULL;

    shogi_model_reset();

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "New model created.");

    return model;
}

void shogi_model_close() {
    fclose(model->history);
    remove(".shogi_history.bin");
}

gboolean shogi_model_is_black_turn() {
    return is_black_turn;
}

enum SHOGI_MODEL_MODE shogi_model_get_mode() {
    return mode;
}

enum SHOGI_PAWN_DETAILED **shogi_model_get_board() {
    return model->board;
}

char **shogi_model_get_available_moves() {
    return available_moves;
}

int *shogi_model_get_hand(gboolean is_white) {
    return is_white ? model->hand[0] : model->hand[1];
}

gint64 shogi_model_timer_get_time(gboolean is_white) {
    return model->timer[is_white ? 0 : 1];
}

gboolean shogi_model_is_check(enum SHOGI_PAWN_DETAILED **board, gboolean check_for_black) {
    char **hitmap = shogi_model_hitmap_new();
    shogi_model_hitmap_calc_all(hitmap, board, check_for_black);

    gboolean in_check = FALSE;

    for (int i = 1; !in_check && i <= 9; ++i) // search for king
        for (int j = 1; !in_check && j <= 9; ++j)
            if (SHOGI_PAWN_TO_BASE_TYPE(IDX(board, j, i)) == SHOGI_PAWN_K && // if we found king
                SHOGI_MODEL_MINE_OCCUPIES(board, COL(j), ROW(i), check_for_black) && // if it is our color
                IDX(hitmap, j, i) == 'o')    // if field on king is reachable
                in_check = TRUE;

    // deallocate
    SHOGI_MODEL_FREE_MATRIX(hitmap);
    // ----------

    return in_check;

}


//----------------------------------------------------------------------------------------------------------------------


gboolean shogi_model_click(int col, int row) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Executed shogi_model_click(%d, %d)", col, row);
    if (col < 1 || col > 9 || row < 1 || row > 9) {
        return FALSE;
    }


    if (mode == NONE) { /// player selected a piece on board
        if (SHOGI_MODEL_MINE_OCCUPIES(model->board, COL(col), ROW(row), is_black_turn)) { // if mine, select for move
            available_moves = shogi_model_hitmap_calc(available_moves, model->board, col,
                                                      row); // calculate available moves map
            selected_col = col;
            selected_row = row;
            selected_pawn = IDX(model->board, col, row);
            mode = MOVE;
            return TRUE;
        } else  // else do nothing
            return FALSE;


    } else if (mode == DROP) { /// player drops pawn
        if (IDX(available_moves, col, row) == 'o') { // if it can be dropped here. Was calculated by _drop_mode()
            assert(IDX(model->board, col, row) == SHOGI_PAWN_DETAILED_NONE); // assert against drop on pawns
            IDX(model->board, col, row) = selected_pawn; // override current pos
            model->hand[is_black_turn ? 1 : 0][selected_pawn / 2]--; // upd hand
            char *move = parse_move(selected_pawn, -1, -1, 2, col, row, 0);
            char *state = shogi_model_serialize_state();
            append_history(move, hash_string(state), state);
            free(move);
            free(state);
            change_player(); // nothing happens next, change player
            return TRUE;
        }
        shogi_model_hitmap_clear(available_moves); // clear for renderer on improper placement. move to initial state
        mode = NONE;
        selected_pawn = SHOGI_PAWN_DETAILED_NONE; // set selected_pawn to none, so UI can act accordingly
        return TRUE;


    } else if (mode == MOVE) { /// move piece
        if (IDX(available_moves, col, row) == 'o') { // was calculated by hitmap_calc() during mode setup
            mode = NONE;
            previous_captured = FALSE;
            shogi_model_hitmap_clear(available_moves);
            if (IDX(model->board, col, row) != SHOGI_PAWN_DETAILED_NONE) { /// capture enemy's pawn
                enum SHOGI_PAWN_DETAILED captured = IDX(model->board, col, row);

                if (SHOGI_PAWN_TO_BASE_TYPE(captured) == SHOGI_PAWN_K) { // win on king capture
                    mode = is_black_turn ? BLACK_WIN : WHITE_WIN;
                    IDX(model->board, col, row) = selected_pawn;
                    IDX(model->board, selected_col, selected_row) = SHOGI_PAWN_DETAILED_NONE;
                    char *move = parse_move(selected_pawn, selected_col, selected_row, 1, col, row, 0);
                    char *state = shogi_model_serialize_state();
                    append_history(move, hash_string(state), state);
                    free(move);
                    free(state);
                    return TRUE; // return true and wait for promote mode
                }

                if (SHOGI_PAWN_IS_PROMOTED(captured)) // add pawn to players hand
                    captured -= SHOGI_PAWN_PRO_OFFSET;
                model->hand[is_black_turn ? 1 : 0][SHOGI_PAWN_TO_BASE_TYPE(captured)]++;

                previous_captured = TRUE; // for history append
            }

            IDX(model->board, col, row) = selected_pawn; // place pawn at new spot
            IDX(model->board, selected_col, selected_row) = SHOGI_PAWN_DETAILED_NONE; // set previous pos to empty

            if (pawn_can_possibly_move(row, selected_pawn) &&  /// check if pawn can be promoted
                pawn_can_promote(col, row, selected_col, selected_row) &&
                SHOGI_PAWN_DETAILED_IS_PROMOTABLE(selected_pawn)) {

                mode = PROMOTING;
                previous_col = selected_col; // save for history append
                previous_row = selected_row;
                selected_col = col;
                selected_row = row;
                return TRUE; // if it can promote, change mode to PROMOTING and exit, wait for dialog popup
            }
            if (!pawn_can_possibly_move(row, selected_pawn)) { /// if it cannot move afterwards, compulsory promote
                previous_col = selected_col; // save for history append
                previous_row = selected_row;
                selected_col = col;
                selected_row = row;
                shogi_model_promote(TRUE);
                return TRUE;
            }
            // simple move without promote and capture
            char *move = parse_move(selected_pawn, selected_col, selected_row, previous_captured, col, row, 0);
            char *state = shogi_model_serialize_state();
            append_history(move, hash_string(state), state);
            free(move);
            free(state);
            change_player();
            return TRUE;
        }
        shogi_model_hitmap_clear(available_moves);
        mode = NONE;
        selected_pawn = SHOGI_PAWN_DETAILED_NONE;
        return TRUE;
    }
}

gboolean shogi_model_drop_mode(enum SHOGI_PAWN_DETAILED pawn) {
    if (mode == WHITE_WIN || mode == BLACK_WIN) return FALSE;
    // if hand is empty, return
    if (model->hand[is_black_turn ? 1 : 0][pawn / 2] == 0) return FALSE;

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Entered drop mode for enum SHOGI_PAWN_DETAILED = %d", pawn);

    if (mode == DROP && pawn == selected_pawn) {
        mode = NONE;
        shogi_model_hitmap_clear(available_moves);
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Exited drop mode for enum SHOGI_PAWN_DETAILED = %d", pawn);
        return true;
    }

    mode = DROP;
    // fill all without occupied and
    for (int i = 1; i <= 9; ++i) {
        for (int j = 1; j <= 9; ++j) {
            // exclude rows for N,L,P and occupied tiles
            if (SHOGI_MODEL_IS_FREE(model->board, COL(j), ROW(i)) && pawn_can_possibly_move(i, pawn))
                IDX(available_moves, j, i) = 'o';
        }
    }

    /// rules for dropping pawn
    if (pawn / 2 == SHOGI_PAWN_P) {
        // check occupied columns for pawn
        for (int i = 1; i <= 9; ++i) {
            for (int j = 1; j <= 9; ++j) {
                if (IDX(model->board, j, i) == pawn)
                    for (int k = 1; k <= 9; ++k) // clear column
                        IDX(available_moves, j, k) = ' ';
            }
        }

        // exclude squares that cause drop mate
        shogi_model_exclude_drop_mate(model->board, available_moves, is_black_turn);
    }
    selected_pawn = pawn;

    return TRUE;
}

void shogi_model_promote(gboolean want_promote) {
    mode = NONE;

    if (want_promote) {
        assert(SHOGI_PAWN_DETAILED_IS_PROMOTABLE(IDX(model->board, selected_col, selected_row))); // paranoia check
        IDX(model->board, selected_col, selected_row) += SHOGI_PAWN_PRO_OFFSET;
    }
    char *move = parse_move(selected_pawn,
                            previous_col, previous_row,
                            previous_captured ? 1 : 0,
                            selected_col, selected_row,
                            want_promote ? 1 : 2);
    char *state = shogi_model_serialize_state();
    append_history(move, hash_string(state), state);
    free(move);
    free(state);
    change_player();
}

void shogi_model_reset() {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Resetting board to initial state.");
    is_black_turn = TRUE;
    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < 9; ++j)
            model->board[i][j] = SHOGI_PAWN_DETAILED_NONE;

    IDX(model->board, 1, 1) = SHOGI_PAWN_DETAILED_L_WHITE;
    IDX(model->board, 2, 1) = SHOGI_PAWN_DETAILED_N_WHITE;
    IDX(model->board, 3, 1) = SHOGI_PAWN_DETAILED_S_WHITE;
    IDX(model->board, 4, 1) = SHOGI_PAWN_DETAILED_G_WHITE;
    IDX(model->board, 5, 1) = SHOGI_PAWN_DETAILED_K_WHITE;
    IDX(model->board, 6, 1) = SHOGI_PAWN_DETAILED_G_WHITE;
    IDX(model->board, 7, 1) = SHOGI_PAWN_DETAILED_S_WHITE;
    IDX(model->board, 8, 1) = SHOGI_PAWN_DETAILED_N_WHITE;
    IDX(model->board, 9, 1) = SHOGI_PAWN_DETAILED_L_WHITE;

    IDX(model->board, 2, 2) = SHOGI_PAWN_DETAILED_B_WHITE;
    IDX(model->board, 8, 2) = SHOGI_PAWN_DETAILED_R_WHITE;

    for (int i = 1; i <= 9; ++i)
        IDX(model->board, i, 3) = SHOGI_PAWN_DETAILED_P_WHITE;

    IDX(model->board, 1, 9) = SHOGI_PAWN_DETAILED_L_BLACK;
    IDX(model->board, 2, 9) = SHOGI_PAWN_DETAILED_N_BLACK;
    IDX(model->board, 3, 9) = SHOGI_PAWN_DETAILED_S_BLACK;
    IDX(model->board, 4, 9) = SHOGI_PAWN_DETAILED_G_BLACK;
    IDX(model->board, 5, 9) = SHOGI_PAWN_DETAILED_K_BLACK;
    IDX(model->board, 6, 9) = SHOGI_PAWN_DETAILED_G_BLACK;
    IDX(model->board, 7, 9) = SHOGI_PAWN_DETAILED_S_BLACK;
    IDX(model->board, 8, 9) = SHOGI_PAWN_DETAILED_N_BLACK;
    IDX(model->board, 9, 9) = SHOGI_PAWN_DETAILED_L_BLACK;

    IDX(model->board, 2, 8) = SHOGI_PAWN_DETAILED_R_BLACK;
    IDX(model->board, 8, 8) = SHOGI_PAWN_DETAILED_B_BLACK;

    for (int i = 1; i <= 9; ++i)
        IDX(model->board, i, 7) = SHOGI_PAWN_DETAILED_P_BLACK;

    for (int i = 0; i < SHOGI_PAWN_COUNT; ++i)
        model->hand[0][i] = model->hand[1][i] = 0;

    mode = NONE;
    // reset temporary data
    selected_pawn = SHOGI_PAWN_DETAILED_NONE;
    shogi_model_hitmap_clear(available_moves);
    selected_col = 0;
    selected_row = 0;
    shogi_model_timer_set(0);

    // reset history
    model->history_entries = 0;
    if (model->history) {
        fclose(model->history);
        remove(".shogi_history.bin");
    }
    model->history = fopen(".shogi_history.bin", "wb+");
    if (model->history == NULL)
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_WARN, "Unable to open .shogi_history.bin in wb+ mode.");

}

char **shogi_model_hitmap_new() {
    char **hitmap = calloc(9, sizeof(char *));
    for (int i = 0; i < 9; ++i) hitmap[i] = calloc(9, sizeof(char));
    shogi_model_hitmap_clear(hitmap);
    return hitmap;
}

char **shogi_model_hitmap_calc(char **hitmap, enum SHOGI_PAWN_DETAILED **board, int col, int row) {

    // first of all, clear our hitmap;
    shogi_model_hitmap_clear(hitmap);

    enum SHOGI_PAWN_DETAILED pawn = IDX(board, col, row); // get pawn at given place on board
    if (pawn == SHOGI_PAWN_NONE) return hitmap; // if no pawn is at given place, return empty hitmap

    gboolean black_turn = SHOGI_PAWN_IS_BLACK(pawn); // which pawn we calculate

    int origin_col = COL(col), origin_row = ROW(row); // absolute indexes in matrix

    // searching for pawn pos in pattern corresponding to pawn
    int x_col = -1, x_row = -1; // 0 0 is upper left corner unlike on board
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            if (pawn_move_pattern[pawn / 2][r][c] == 'x') {
                x_row = r;
                x_col = c;
            }

    assert(x_col != -1 && x_row != -1); // assert against pattern without 'x'

    // filling matrix with symbols
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            if (pawn_move_pattern[pawn / 2][r][c] != ' ' && // if pattern specifies this field...
                pawn_move_pattern[pawn / 2][r][c] != 'x') { // ...

                // translated row and column to corresponding on pattern
                int tr = origin_row + ((black_turn) ? r - x_row : (r - x_row) * -1); // translated row
                int tc = origin_col + ((black_turn) ? c - x_col : (c - x_col) * -1); // translated column

                if (SHOGI_MODEL_CORDS_GUT(tc, tr)) { // if translated coordinates are on board
                    if (pawn_move_pattern[pawn / 2][r][c] == 'o') { // if field is of type "jump"
                        if (SHOGI_MODEL_OTHER_OCCUPIES(board, tc, tr, black_turn) || SHOGI_MODEL_IS_FREE(board, tc, tr))
                            hitmap[tr][tc] = 'o';

                    } else { // line patterns
                        int col_step = 0;
                        int row_step = 0;
                        switch (pawn_move_pattern[pawn / 2][r][c]) {
                            case '-':
                                col_step = tc > origin_col ? 1 : -1;
                                break;
                            case '|':
                                row_step = tr > origin_row ? 1 : -1;
                                break;
                            case '/':
                                col_step = tc > origin_col ? 1 : -1;
                                row_step = tc > origin_col ? -1 : 1;
                                break;
                            case '\\':
                                col_step = tc > origin_col ? 1 : -1;
                                row_step = tc > origin_col ? 1 : -1;
                                break;
                            default:
                                break;
                        }
                        int current_col = tc;
                        int current_row = tr;
                        while (SHOGI_MODEL_CORDS_GUT(current_col, current_row)) { // fill in whole rows
                            if (SHOGI_MODEL_IS_FREE(board, current_col, current_row))
                                hitmap[current_row][current_col] = 'o';
                            if (SHOGI_MODEL_OTHER_OCCUPIES(board, current_col, current_row, black_turn)) {
                                hitmap[current_row][current_col] = 'o';
                                break;
                            }
                            if (SHOGI_MODEL_MINE_OCCUPIES(board, current_col, current_row, black_turn))
                                break;
                            current_col += col_step;
                            current_row += row_step;
                        }
                    }
                }
            }
        }
    }

    hitmap[origin_row][origin_col] = 'x';
    return hitmap;
}

char **shogi_model_hitmap_calc_all(char **hitmap, enum SHOGI_PAWN_DETAILED **board, gboolean blacks) {


    shogi_model_hitmap_clear(hitmap);
    char **hitmap_mask = shogi_model_hitmap_new();


    // override mask for all pieces of the opponent
    for (int i = 1; i <= 9; ++i) {
        for (int j = 1; j <= 9; ++j) {
            if (SHOGI_MODEL_OTHER_OCCUPIES(board, COL(j), ROW(i), blacks)) { // if piece of oponent found:
                shogi_model_hitmap_clear(hitmap_mask);                         // calc hitmap mask for this piece...
                hitmap_mask = shogi_model_hitmap_calc(hitmap_mask, board, j, i);  // ...
                for (int k = 0; k < 9; ++k) // OR hitmap and mask       // OR mask and hitmap
                    for (int l = 0; l < 9; ++l)
                        hitmap[k][l] = (char) (hitmap_mask[k][l] == 'o' ? 'o' : hitmap[k][l]);

            }
        }
    }

    for (int i = 0; i < 9; ++i) // dealloc hitmap_mask
        free(hitmap_mask[i]);
    free(hitmap_mask);

    return hitmap;
}

void shogi_model_hitmap_clear(char **mask) {
    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < 9; ++j)
            mask[i][j] = ' ';
}

void shogi_model_timer_set(guint32 initial_time) {
    if (initial_time == 0) model->TIMED_MODE = FALSE;
    else model->TIMED_MODE = TRUE;
    model->timer[0] = model->timer[1] = initial_time;
}

void shogi_model_timer_decrease(clock_t delta) {
    if (!model->TIMED_MODE || mode == WHITE_WIN || mode == BLACK_WIN) return;
    model->timer[is_black_turn ? 1 : 0] -= (gint64) (intptr_t) delta;
    if (model->timer[is_black_turn ? 1 : 0] <= 0) {
        model->timer[is_black_turn ? 1 : 0] = 0;
        mode = is_black_turn ? WHITE_WIN : BLACK_WIN;
        shogi_model_hitmap_clear(available_moves);
        model->TIMED_MODE = FALSE;
    }

}

void shogi_model_resign() {
    mode = is_black_turn ? WHITE_WIN : BLACK_WIN;
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_INFO, "Player resigned");
}


//----------------------------------------------------------------------------------------------------------------------


inline static void change_player() {
    is_black_turn = !is_black_turn;
    selected_pawn = SHOGI_PAWN_DETAILED_NONE;
    selected_col = 0;
    selected_row = 0;

    shogi_model_hitmap_clear(available_moves); // clear available moves map for renderer
    // todo: timers
}

inline static gboolean pawn_can_possibly_move(int row, enum SHOGI_PAWN_DETAILED pawn) {
    if (pawn == SHOGI_PAWN_DETAILED_N_BLACK && row <= 2) return FALSE;
    if (pawn == SHOGI_PAWN_DETAILED_N_WHITE && row >= 8) return FALSE;
    if (pawn == SHOGI_PAWN_DETAILED_L_BLACK && row == 1) return FALSE;
    if (pawn == SHOGI_PAWN_DETAILED_L_WHITE && row == 9) return FALSE;
    if (pawn == SHOGI_PAWN_DETAILED_P_BLACK && row == 1) return FALSE;
    if (pawn == SHOGI_PAWN_DETAILED_P_WHITE && row == 9) return FALSE;
    return TRUE;
}

inline static gboolean pawn_can_promote(int colA, int rowA, int colB, int rowB) {
    if (colA == colB && rowA == rowB) return FALSE; // if the same place, false. Maybe assert against ?
    // from now on two places are different
    if (is_black_turn && (rowA <= 3 || rowB <= 3)) return TRUE; // if move was in a part of rows <= 3
    if (!is_black_turn && (rowA >= 7 || rowB >= 7)) return TRUE; // if move was in a part of rows >= 7
    return FALSE;
}

static void shogi_model_exclude_drop_mate(enum SHOGI_PAWN_DETAILED **board, char **hitmap, gboolean black_drops) {


    // searching for opposing king
    int K_col = -1;
    int K_row = -1;
    for (int i = 1; i <= 9; ++i)
        for (int j = 1; j <= 9; ++j)
            if (SHOGI_MODEL_OTHER_OCCUPIES(board, COL(j), ROW(i), black_drops) &&
                SHOGI_PAWN_TO_BASE_TYPE(IDX(board, j, i)) == SHOGI_PAWN_K) {
                K_col = j;
                K_row = i;
            }

    // setting coordinates for placed pawn
    int P_col = K_col;
    int P_row = K_row + (black_drops ? 1 : -1);

    if ((black_drops && K_row == 9) || (!black_drops && K_row == 1))
        return; // king in last rows => impossible to dropmate
    if (IDX(hitmap, P_col, P_row) != 'o') return; // ahead of king is occupied

    // making working copy of board
    enum SHOGI_PAWN_DETAILED **board_copy = calloc(9, sizeof(enum SHOGI_PAWN_DETAILED *));

    for (int i = 0; i < 9; ++i)
        board_copy[i] = calloc(9, sizeof(enum SHOGI_PAWN_DETAILED));
    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < 9; ++j)
            board_copy[i][j] = board[i][j];

    // pawn can checkmate king only if it is dropped in front of him, so 3 scenarios are possible:
    // #1 if king can capture checking pawn without check, then it is ok

    // #2 if king can run away and not be in check, it is viable move
    // #3 if other pawn can kill checking pawn and king is not in check, that's ok

    IDX(board_copy, P_col, P_row) = SHOGI_PAWN_TO_DETAILED_TYPE(SHOGI_PAWN_P, black_drops); // put dummy pawn

    // #1 and #2 - king moves
    // check available moves for king, try them and check if it is check

    char **king_hitmap = shogi_model_hitmap_new();
    shogi_model_hitmap_calc(king_hitmap, board_copy, K_col, K_row); // generate hitmap for king

    for (int i = 1; i <= 9; ++i) {
        for (int j = 1; j < 9; ++j) {
            if (IDX(king_hitmap, j, i) == 'o') { //for all possible moves for king
                enum SHOGI_PAWN_DETAILED original_piece = IDX(board_copy, j, i); // save the piece for later undo
                IDX(board_copy, j, i) = IDX(board_copy, K_col, K_row); //move king there
                IDX(board_copy, K_col, K_row) = SHOGI_PAWN_DETAILED_NONE;
                if (!shogi_model_is_check(board_copy, !black_drops)) { // If enemy pawn is no longer in check...
                    SHOGI_MODEL_FREE_MATRIX(king_hitmap);               // it is a valid move, so exit freeing resources
                    SHOGI_MODEL_FREE_MATRIX(board_copy);
                    return;
                }
                IDX(board_copy, K_col, K_row) = IDX(board_copy, j, i); // move king back
                IDX(board_copy, j, i) = original_piece; // restore piece at that spot
            }
        }
    }
    SHOGI_MODEL_FREE_MATRIX(king_hitmap);

    // #3 check if we can hit pawn with other piece and move king out of check

    char **piece_hitmap = shogi_model_hitmap_new(); // hitmap for checking if piece can capture the pawn
    for (int i = 1; i <= 9; ++i) {
        for (int j = 1; j < 9; ++j) {
            if (SHOGI_MODEL_OTHER_OCCUPIES(board_copy, COL(j), ROW(i), black_drops)) { // for all enemy's pawns
                shogi_model_hitmap_calc(piece_hitmap, board, j, i); // generate hitmap for that piece.
                if (IDX(piece_hitmap, P_col, P_row) == 'o') { // if given piece can capture attacking pawn...
                    enum SHOGI_PAWN_DETAILED original_piece = IDX(board_copy, P_col,
                                                                  P_row); // save the piece for later undo
                    IDX(board_copy, P_col, P_row) = IDX(board_copy, j, i); //move this pawn there...
                    IDX(board_copy, j, i) = SHOGI_PAWN_DETAILED_NONE; // remove it from previous position...
                    if (!shogi_model_is_check(board_copy, !black_drops)) { // king is no longer in check - OK
                        SHOGI_MODEL_FREE_MATRIX(piece_hitmap);
                        SHOGI_MODEL_FREE_MATRIX(board_copy);
                        return;
                    }
                    IDX(board_copy, j, i) = IDX(board_copy, P_col, P_row); // move piece back
                    IDX(board_copy, P_col, P_row) = original_piece; // restore captured pawn
                }
            }
        }
    }
    SHOGI_MODEL_FREE_MATRIX(piece_hitmap);
    // if we reached here, it means that move is illegal
    IDX(hitmap, P_col, P_row) = ' ';
}

inline static unsigned long hash_string(char *str) {
    HASH hash = 0;
    int c;

    while (c = *str++)
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

char *shogi_model_serialize_state() { // TODO check corrupted files
    char *state = calloc(SHOGI_MODEL_SERIALIZED_STATE_LENGTH, sizeof(char));
    if (state == NULL) {
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_ERROR, "Cannot allocate memory for model serialization string.");
        return NULL;
    }
    for (int i = 0; i < 8; ++i) {
        state[i] = SHOGI_MODEL_TO_COUNT_CODE(model->hand[0][i]);
        state[8 + i] = SHOGI_MODEL_TO_COUNT_CODE(model->hand[1][i]);
    }
    for (int i = 0; i < 81; ++i)
        state[16 + i] = SHOGI_MODEL_TO_PAWN_CODE(model->board[i / 9][i % 9]);
    state[SHOGI_MODEL_SERIALIZED_STATE_LENGTH - 1] = '\0';
    return state;
}

void shogi_model_deserialize_state(const char *state) {
    for (int i = 0; i < 8; ++i) {
        model->hand[0][i] = SHOGI_MODEL_FROM_COUNT_CODE(state[i]);
        model->hand[1][i] = SHOGI_MODEL_FROM_COUNT_CODE(state[8 + i]);
    }
    for (int i = 0; i < 81; ++i)
        model->board[i / 9][i % 9] = SHOGI_MODEL_FROM_PAWN_CODE(state[16 + i]);
}

void shogi_model_load_game(FILE *file) {
    // reset model to clear state
    shogi_model_reset();

    // load structure
    // [state][black_turn][timed][timer[2]][entries][{history}]
    char state[SHOGI_MODEL_SERIALIZED_STATE_LENGTH];
    fread(&state, sizeof(char), SHOGI_MODEL_SERIALIZED_STATE_LENGTH, file);
    shogi_model_deserialize_state(state);
    fread(&is_black_turn, sizeof(gboolean), 1, file); // if black turn
    fread(&model->TIMED_MODE, sizeof(gboolean), 1, file); // if timed mode
    fread(&model->timer, sizeof(gint64), 2, file); // timers time

    fread(&model->history_entries, sizeof(int), 1, file); // entries in history
    fseek(model->history, 0, SEEK_SET); // rewind history file to the beginning
    for (int i = 0; i < model->history_entries; ++i) { // copy history
        ShogiModelHistoryEntry entry;
        fread(&entry, sizeof(ShogiModelHistoryEntry), 1, file);
        fwrite(&entry, sizeof(ShogiModelHistoryEntry), 1, model->history);
    }
    fseek(model->history, 0, SEEK_END); // rewind history file to the end
}

inline static char *
parse_move(enum SHOGI_PAWN_DETAILED pawn, int colA, int rowA, int type, int colB, int rowB, int promote) {
    assert(pawn != SHOGI_PAWN_DETAILED_NONE);
    char *move = calloc(SHOGI_MODEL_MOVE_LENGTH, sizeof(char));
    if (!move) {
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_ERROR, "Couldn't create new string representing move.");
        return NULL;
    }
    int it = 0;
    if (SHOGI_PAWN_IS_PROMOTED(pawn))
        move[it++] = '+';
    move[it++] = pawn_base_character[SHOGI_PAWN_TO_BASE_TYPE(pawn)];
    if (colA != -1 && colB != -1) {
        move[it++] = (char) ('0' + colA);
        move[it++] = (char) ('0' + rowA);
    }
    if (type == 0)
        move[it++] = '-';
    else if (type == 1)
        move[it++] = 'x';
    else if (type == 2)
        move[it++] = '*';

    move[it++] = (char) ('0' + colB);
    move[it++] = (char) ('0' + rowB);
    if (promote == 1)
        move[it++] = '+';
    else if (promote == 2)
        move[it++] = '=';

    move[it++] = 0;
    return move;
}

inline static void append_history(const char *move, HASH hash, const char *state_serialized) {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Writing entry to history.");
    if (model->history == NULL) {
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_WARN, "Cannot append to history: file is NULL.");
        return;
    }
    if (move == NULL || state_serialized == NULL) {
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_WARN,
                         "Cannot append to history: one of move or state_serialized is NULL.");
        return;
    }

    ShogiModelHistoryEntry entry;
    for (int i = 0; i < SHOGI_MODEL_MOVE_LENGTH; ++i)
        entry.move[i] = move[i];
    entry.hash = hash;
    for (int i = 0; i < SHOGI_MODEL_SERIALIZED_STATE_LENGTH; ++i)
        entry.state[i] = state_serialized[i];

    // write structure to binary file file as new entry
    fwrite(&entry, sizeof(ShogiModelHistoryEntry), 1, model->history);
    model->history_entries++;

    //printf("%s|%lu|%s\n", entry.move, hash, entry.state);
    fflush(model->history);

}
