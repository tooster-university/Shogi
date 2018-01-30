//
// Created by Tooster on 22.01.2018.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "Model.h"
#include "Logger.h"

enum SHOGI_MODEL_MODE mode = NONE;
enum SHOGI_PAWN_DETAILED active = SHOGI_PAWN_DETAILED_NONE;
int selected_col = 0;
int selected_row = 0;

static gboolean is_black_turn = TRUE;
static ShogiModel *model;
// TODO: Timers
// @formatter:off
// available moves pattern, overwritten in recalc_avail_moves()
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
// @formatter:on

// macros require absolute indexes in matrix (0..8 left to right)
#define SHOGI_MODEL_CORDS_GUT(col_ix, row_ix) ( (row_ix) >= 0 && (row_ix) < 9 && (col_ix) >= 0 && (col_ix) < 9 )
#define SHOGI_MODEL_MINE_OCCUPIES(col_ix, row_ix) ((is_black_turn && SHOGI_PAWN_IS_BLACK(model->board [row_ix][col_ix])) || \
                                            (!is_black_turn && SHOGI_PAWN_IS_WHITE(model->board [row_ix][col_ix])))
#define SHOGI_MODEL_OTHER_OCCUPIES(col_ix, row_ix) ((is_black_turn && SHOGI_PAWN_IS_WHITE(model->board [row_ix][col_ix])) || \
                                            (!is_black_turn && SHOGI_PAWN_IS_BLACK(model->board [row_ix][col_ix])))
#define SHOGI_MODEL_IS_FREE(col_ix, row_ix) (model->board[row_ix][col_ix] == SHOGI_PAWN_NONE)

#define _OR(c1, c2) (((c1) == 'o' || (c2) == 'o')? 'o' : ' ')

//----------------------------------------------------------------------------------------------------------------------


ShogiModel *shogi_model_init() {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Creating new model...");
    model = malloc(sizeof(ShogiModel));
    if (model == NULL)
        return NULL;

    model->black_hand = calloc(SHOGI_PAWN_COUNT, sizeof(int));
    model->white_hand = calloc(SHOGI_PAWN_COUNT, sizeof(int));
    model->board = calloc(9, sizeof(enum SHOGI_PAWN_DETAILED *));
    if (model->black_hand == NULL || model->white_hand == NULL || model->board == NULL) {
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_FATAL, "One of model's fields couldn't be allocated.");
        free(model->black_hand);
        free(model->white_hand);
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
            free(model->black_hand);
            free(model->white_hand);
            free(model->board);
            free(model);
            return NULL;
        }
    }

    available_moves = calloc(9, sizeof(char *));
    for (int j = 0; j < 9; ++j)
        available_moves[j] = calloc(9, sizeof(char));

    clear_avail_moves();

    shogi_model_reset();

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "New model created.");

    return model;
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
    return is_white ? model->white_hand : model->black_hand;
}

gboolean shogi_model_is_check(gboolean on_black) {
    char **hitmap_mask = calloc(9, sizeof(char *));
    for (int i = 0; i < 9; ++i) hitmap_mask[i] = calloc(9, sizeof(char));

    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < 9; ++j)
            hitmap_mask[i][j] = ' ';

    // deallocate
    for (int i = 0; i < 9; ++i)
        free(hitmap_mask[i]);
    free(hitmap_mask);
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_WARN, "Watch out, is_check function is not implemented yet! ");
    return FALSE;

}

gboolean shogi_model_is_mate(gboolean on_black) {
//    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "available moves pattern for pawn = %d:", -1);
//    for (int i = 0; i < 9; ++i)
//        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "[%c][%c][%c][%c][%c][%c][%c][%c][%c]",
//                         available_moves[i][0],
//                         available_moves[i][1],
//                         available_moves[i][2],
//                         available_moves[i][3],
//                         available_moves[i][4],
//                         available_moves[i][5],
//                         available_moves[i][6],
//                         available_moves[i][7],
//                         available_moves[i][8]
//        );
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_WARN, "Watch out, is_mate function is not implemented yet! ");
    return FALSE; // TODO;

}


//----------------------------------------------------------------------------------------------------------------------


gboolean shogi_model_click(int col, int row) {
    if (col < 1 || col > 9 || row < 1 || row > 9) {
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Executed shogi_model_click(%d, %d)", col, row);
        return FALSE;
    }
    if (mode == NONE) {
        if (SHOGI_MODEL_MINE_OCCUPIES(COL(col), ROW(row))) {
            recalc_avail_moves(col, row); // calculate available moves
            selected_col = col;
            selected_row = row;
            active = model->board _IDX(col, row);
            mode = MOVE;
            return TRUE;
        } else
            return FALSE;


    } else if (mode == DROP) {
        if (available_moves _IDX(col, row) == 'o') { // if it can be dropped here. It was calculated by _drop_mode()

            model->board _IDX(col, row) = active;
            is_black_turn ? model->black_hand[active / 2]-- : model->white_hand[active / 2]--;
            change_player();
            clear_avail_moves();
            return TRUE;
        }
        clear_avail_moves();
        mode = NONE;
        active = SHOGI_PAWN_DETAILED_NONE; // set active to none, so UI can act accordingly
        return TRUE;
    } else if (mode == MOVE) {
        if (available_moves _IDX(col, row) == 'o') { // was calculated by recalc_avail in initialization of mode
            mode = NONE;
            clear_avail_moves();
            if (model->board _IDX(col, row) != SHOGI_PAWN_DETAILED_NONE) { // capture enemy's pawn
                enum SHOGI_PAWN_DETAILED captured = model->board _IDX(col, row);

                if (captured / 2 == SHOGI_PAWN_K) {
                    mode = is_black_turn ? BLACK_WIN : WHITE_WIN;
                    model->board _IDX(col, row) = active;
                    model->board _IDX(selected_col, selected_row) = SHOGI_PAWN_DETAILED_NONE;
                    return TRUE;
                }

                if (captured >= SHOGI_PAWN_DETAILED_S_PRO_WHITE)
                    captured -= SHOGI_PAWN_PRO_OFFSET;
                captured /= 2;
                if (is_black_turn)
                    model->black_hand[captured]++;
                else
                    model->white_hand[captured]++;
            }
            model->board _IDX(col, row) = active;
            model->board _IDX(selected_col, selected_row) = SHOGI_PAWN_DETAILED_NONE;
            if (pawn_can_possibly_move(row, active) &&
                pawn_can_promote(col, row, selected_col, selected_row) &&
                SHOGI_PAWN_DETAILED_IS_PROMOTABLE(active)) {

                mode = PROMOTING;
                selected_col = col;
                selected_row = row;
                return TRUE;
            }
            if (!pawn_can_possibly_move(row, active)) {
                selected_col = col;
                selected_row = row;
                shogi_model_promote(TRUE);
                return TRUE;
            }
            change_player();
            return TRUE;
        }
        clear_avail_moves();
        mode = NONE;
        active = SHOGI_PAWN_DETAILED_NONE;
        return TRUE;
    }
}

gboolean shogi_model_drop_mode(enum SHOGI_PAWN_DETAILED pawn) {
    if (mode == WHITE_WIN || mode == BLACK_WIN) return FALSE;
    // if hand is empty, return
    if (is_black_turn && model->black_hand[pawn / 2] == 0) return FALSE;
    else if (!is_black_turn && model->white_hand[pawn / 2] == 0) return FALSE;

    if(mode == DROP && pawn == active){
        mode = NONE;
        clear_avail_moves();
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Exited drop mode for enum SHOGI_PAWN_DETAILED = %d", pawn);
        return true;
    }

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Entered drop mode for enum SHOGI_PAWN_DETAILED = %d", pawn);
    mode = DROP;
    // fill all without occupied and
    for (int i = 1; i <= 9; ++i) {
        for (int j = 1; j <= 9; ++j) {
            // exclude rows for N,L,P and occupied tiles
            if (SHOGI_MODEL_IS_FREE(COL(j), ROW(i)) && pawn_can_possibly_move(i, pawn))
                available_moves _IDX(j, i) = 'o';
        }
    }
    fflush(stdout);
    if (pawn / 2 == SHOGI_PAWN_P) {
        // check occupied columns for pawn
        for (int i = 1; i <= 9; ++i) {
            for (int j = 1; j <= 9; ++j) {
                if (model->board _IDX(j, i) == pawn)
                    for (int k = 1; k <= 9; ++k) // clear column
                        available_moves _IDX(j, k) = ' ';
            }
        }

        for (int i = 1; i <= 9; ++i) {
            for (int j = 1; j <= 9; ++j) {
                if (available_moves _IDX(j, i) == 'o') {
                    // if it isn't we have mismatch in avail_move calculation
                    assert(model->board
                                   _IDX(j, i) == SHOGI_PAWN_DETAILED_NONE); // FIXME
                    model->board _IDX(j, i) = pawn; // fake drop
                    is_black_turn = !is_black_turn; // fake player change
                    if (shogi_model_is_mate(is_black_turn)) // check if this produces mate
                        available_moves _IDX(j, i) = ' ';
                    is_black_turn = !is_black_turn; // undo fake player change
                    model->board _IDX(j, i) = SHOGI_PAWN_DETAILED_NONE; // undo fake drop
                }
            }
        }
    }
    active = pawn;

    return TRUE;
}

void shogi_model_promote(gboolean want_promote) {
    mode = NONE;

    if (want_promote) {
        assert(SHOGI_PAWN_DETAILED_IS_PROMOTABLE(model->board
                       _IDX(selected_col, selected_row))); // paranoia check
        model->board _IDX(selected_col, selected_row) += SHOGI_PAWN_PRO_OFFSET;
    }
    change_player();
}

void shogi_model_clear_history() {
    // TODO
}

void shogi_model_reset() {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Resetting board to initial state");
    shogi_model_clear_history();
    is_black_turn = TRUE;
    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < 9; ++j)
            model->board[i][j] = SHOGI_PAWN_DETAILED_NONE;

    model->board _IDX(1, 1) = SHOGI_PAWN_DETAILED_L_WHITE;
    model->board _IDX(2, 1) = SHOGI_PAWN_DETAILED_N_WHITE;
    model->board _IDX(3, 1) = SHOGI_PAWN_DETAILED_S_WHITE;
    model->board _IDX(4, 1) = SHOGI_PAWN_DETAILED_G_WHITE;
    model->board _IDX(5, 1) = SHOGI_PAWN_DETAILED_K_WHITE;
    model->board _IDX(6, 1) = SHOGI_PAWN_DETAILED_G_WHITE;
    model->board _IDX(7, 1) = SHOGI_PAWN_DETAILED_S_WHITE;
    model->board _IDX(8, 1) = SHOGI_PAWN_DETAILED_N_WHITE;
    model->board _IDX(9, 1) = SHOGI_PAWN_DETAILED_L_WHITE;

    model->board _IDX(2, 2) = SHOGI_PAWN_DETAILED_B_WHITE;
    model->board _IDX(8, 2) = SHOGI_PAWN_DETAILED_R_WHITE;

    for (int i = 1; i <= 9; ++i)
        model->board _IDX(i, 3) = SHOGI_PAWN_DETAILED_P_WHITE;

    model->board _IDX(1, 9) = SHOGI_PAWN_DETAILED_L_BLACK;
    model->board _IDX(2, 9) = SHOGI_PAWN_DETAILED_N_BLACK;
    model->board _IDX(3, 9) = SHOGI_PAWN_DETAILED_S_BLACK;
    model->board _IDX(4, 9) = SHOGI_PAWN_DETAILED_G_BLACK;
    model->board _IDX(5, 9) = SHOGI_PAWN_DETAILED_K_BLACK;
    model->board _IDX(6, 9) = SHOGI_PAWN_DETAILED_G_BLACK;
    model->board _IDX(7, 9) = SHOGI_PAWN_DETAILED_S_BLACK;
    model->board _IDX(8, 9) = SHOGI_PAWN_DETAILED_N_BLACK;
    model->board _IDX(9, 9) = SHOGI_PAWN_DETAILED_L_BLACK;

    model->board _IDX(2, 8) = SHOGI_PAWN_DETAILED_R_BLACK;
    model->board _IDX(8, 8) = SHOGI_PAWN_DETAILED_B_BLACK;

    for (int i = 1; i <= 9; ++i)
        model->board _IDX(i, 7) = SHOGI_PAWN_DETAILED_P_BLACK;

    for (int i = 0; i < SHOGI_PAWN_COUNT; ++i)
        model->white_hand[i] = model->black_hand[i] = 0;

    mode = NONE;
    active = SHOGI_PAWN_DETAILED_NONE;
    selected_col = 0;
    selected_row = 0;
}


//----------------------------------------------------------------------------------------------------------------------

// recalculates available moves for pawn at given location
static void recalc_avail_moves(int col, int row) {
    clear_avail_moves();

    enum SHOGI_PAWN_DETAILED pawn = model->board _IDX(col, row);
    if (pawn == SHOGI_PAWN_NONE) return; // if no pawn is at given place, return 0

    int origin_col = COL(col), origin_row = ROW(row); // absolute indexes in matrix
    // searching for pawn pos in our pattern
    int x_col = -1, x_row = -1; // 0 0 is upper left corner unlike on board
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            if (pawn_move_pattern[pawn / 2][r][c] == 'x') {
                x_row = r;
                x_col = c;
            }

    assert(x_col != -1 && x_row != -1);
    // filling matrix with symbols

    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            if (pawn_move_pattern[pawn / 2][r][c] != ' ' && pawn_move_pattern[pawn / 2][r][c] != 'x') {

                int dr = r - x_row;
                int dc = c - x_col; // translate c from LR to RL coords
                if (!is_black_turn) { // on white turn rotate 180 degrees
                    dr *= -1;
                    dc *= -1;
                }

                // absolute indexes on array
                int tr = origin_row + dr; // translated row
                int tc = origin_col + dc; // translated column

                if (SHOGI_MODEL_CORDS_GUT(tc, tr)) { // if coordinates are on board
                    if (pawn_move_pattern[pawn / 2][r][c] ==
                        'o') { // different operations for different pattern symbols
                        if (SHOGI_MODEL_OTHER_OCCUPIES(tc, tr) || SHOGI_MODEL_IS_FREE(tc, tr))
                            available_moves[tr][tc] = 'o';

                    } else {
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
                            if (SHOGI_MODEL_IS_FREE(current_col, current_row)) // if it is free, mark gut and continue
                                available_moves[current_row][current_col] = 'o';
                            if (SHOGI_MODEL_OTHER_OCCUPIES(current_col, current_row)) { // if other, mark and break
                                available_moves[current_row][current_col] = 'o';
                                break;
                            }
                            if (SHOGI_MODEL_MINE_OCCUPIES(current_col, current_row)) // if mine, just break
                                break;
                            current_col += col_step;
                            current_row += row_step;
                        }
                    }
                }
            }
        }
    }

    available_moves[origin_row][origin_col] = 'x';

}

inline static void clear_avail_moves() {
    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < 9; ++j)
            available_moves[i][j] = ' ';
}

inline static void change_player() {
    is_black_turn = !is_black_turn;
    active = SHOGI_PAWN_DETAILED_NONE;
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