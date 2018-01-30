//
// Created by Tooster on 22.01.2018.
//

#include <glib.h>
#include "Utils.h"

#ifndef CUWR_MODEL_H
#define CUWR_MODEL_H

typedef char PawnCode; // one of: K, G, S, N, L, B, R, P, Z, M, J, H, D, O (CAPITAL - BLACK, small - white)

// moves are stored in format dxdy as int where dx is from 0 to 20 and midpoint 10 is neutral. dy same as dx but *1000
// dx and dy are delta in rows and columns as seen from black perspective on board

//#define MOVE_PATTERN_DECIPHER_COLUMN(m)
//#define MOVE_PATTERN_DECIPHER_ROW(m)
//#define MOVE_PATTERN_CIPHER(delta_row, delta_column) ()

enum SHOGI_MODEL_MODE {
    NONE,
    MOVE,
    DROP,
    PROMOTING,
    WHITE_WIN,
    BLACK_WIN
};

typedef struct _shogi_model {
    int *white_hand; // hand of white player
    int *black_hand; // hand of black player
    enum SHOGI_PAWN_DETAILED **board; // board of size 9x9 with enums representing pawns
    /// TODO: history
} ShogiModel;

// macros that shift board coordinates to array indexes
#define COL(col) (9-(col))
#define ROW(row) ((row)-1)
#define _IDX(col, row) [ROW(row)][COL(col)] // accepts col and row as in board coordinates (top right = <1,1>)


//----------------------------------------------------------------------------------------------------------------------


/**
 * Creates new model. Returns NULL on failure
 * @return pointer to new model or NULL on failure
 */
ShogiModel *shogi_model_init(); // DONE

/**
 * Returns if it is black's turn
 * @return true if it's black's turn, false otherwise
 */
gboolean shogi_model_is_black_turn(); // DONE

/**
 * Returns current state of model
 * @return current state of model
 */
enum SHOGI_MODEL_MODE shogi_model_get_mode(); // DONE

/**
 * Returns board as it is represented inside model
 * @return
 */
enum SHOGI_PAWN_DETAILED **shogi_model_get_board(); // DONE

/**
 * Returns available moves mask.
 * 'x' is currently selected pawn
 * 'o' is available move
 * ' ' is unavaiable move
 * If player is not in the drop or move state, the mask is filled with ' '
 * @return 9x9 char array with 'x', 'o' and ' ',
 */
char **shogi_model_get_available_moves(); // FIXME

/**
 * Returns the hand of player
 * @param is_white true for white player's hand, false for black's
 * @return pointer to array representing amount of pawn in players hand
 */
int *shogi_model_get_hand(gboolean is_white); // DONE

/**
 * Check if specified player is in check
 * @param on_black true to check if black is in check
 * @return true if check, false otherwise
 */
gboolean shogi_model_is_check(gboolean on_black); // TODO

/**
 * Check if specified player is in checkmate
 * @param on_black true to check if black is in checkmate
 * @return true if checkmate, false otherwise
 */
gboolean shogi_model_is_mate(gboolean on_black); //  TODO

//----------------------------------------------------------------------------------------------------------------------


/**
 * Executed when given field is clicked
 * @param col column on shogi board indexed from 1 to 9
 * @param row row on shogi board indexed from 1 to 9
 * @return true if any change in model happened, false if action was cancelled
 */
gboolean shogi_model_click(int col, int row); // FIXME

/**
 * Enters into drop mode
 * @param pawn pawn to be dropped
 */
gboolean shogi_model_drop_mode(enum SHOGI_PAWN_DETAILED pawn); // FIXME

/**
 * Promotes recently moved piece
 * @warning it does not check for correctness, if pawn at selected location is invalid, assert fails
 * @param want_promote true if promotion is accepted, false if declined
 */
void shogi_model_promote(gboolean want_promote); // DONE

/**
 * Clears history of moves inside model
 */
void shogi_model_clear_history(); // TODO

/**
 * Resets model to initial state
 */
void shogi_model_reset(); // DONE ?

//----------------------------------------------------------------------------------------------------------------------


/**
 * Recalculates available moves for pawn at (col, row)
 * If (0, 0) is passed, the array is cleared
 * @param col
 * @param row
 */
static void recalc_avail_moves(int col, int row); // DONE

/**
 * Clears available moves array
 * @return
 */
inline static void clear_avail_moves(); // DONE

/**
 * Changes current player
 */
inline static void change_player(); // FIXME

/**
 * Returns true if a piece may possibly move, so if pawn is in the last row it cannot etc.
 * @param col col of pawn
 * @param row row of pawn
 * @param pawn pawn
 * @return
 */
inline static gboolean pawn_can_possibly_move(int row, enum SHOGI_PAWN_DETAILED pawn); // DONE

/**
 * Checks if movement from can promote if moved from colA rowA to colB rowB
 * @param colA begin column
 * @param rowA begin row
 * @param colB end column
 * @param rowB end row
 * @return true if pawn can promote
 */
inline static gboolean pawn_can_promote(int colA, int rowA, int colB, int rowB); // DONE
#endif //CUWR_MODEL_H
