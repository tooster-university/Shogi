//
// Created by Tooster on 22.01.2018.
//

#include <glib.h>
#include "Utils.h"

#ifndef CUWR_MODEL_H
#define CUWR_MODEL_H

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

#define SHOGI_MODEL_SERIALIZED_STATE_LENGTH 98
#define SHOGI_MODEL_MOVE_LENGTH 8
typedef unsigned long HASH;


typedef struct _history_entry {

    char move[SHOGI_MODEL_MOVE_LENGTH]; // move in format P63x62+
    HASH hash;    // hash of the state, used to check for sennichite
    char state[SHOGI_MODEL_SERIALIZED_STATE_LENGTH]; // state description
} ShogiModelHistoryEntry;

typedef struct _shogi_model {
    int *hand[2]; // hand of player - [0]=white [1]=black
    enum SHOGI_PAWN_DETAILED **board; // board of size 9x9 with enums representing pawns
    gboolean TIMED_MODE; // true if game is set to mode with timer
    gint64 timer[2]; // timers for players. [0] for white [1] for black
    FILE *history; // binary file holding current history
    int history_entries;
} ShogiModel;

// macros that shift board coordinates to array indexes from notation rows and columns
#define COL(col) (9-(col))
#define ROW(row) ((row)-1)
#define IDX(board, col, row) board[ROW(row)][COL(col)] // accepts col and row as in board coordinates (top right = <1,1>)

#define TO_SECONDS(millis) (((millis) / 1000) % 60)
#define TO_MINUTES(millis) ((millis) / (1000*60))


//----------------------------------------------------------------------------------------------------------------------


/**
 * Creates new model. Returns NULL on failure
 * @return pointer to new model or NULL on failure
 */
ShogiModel *shogi_model_init(); // DONE

/**
 * Runs required actions on close such as file closing and removing
 */
void shogi_model_close();

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
enum SHOGI_PAWN_DETAILED **shogi_model_get_board();

/**
 * Returns available moves mask.
 * 'x' is currently selected pawn
 * 'o' is available move
 * ' ' is unavaiable move
 * If player is not in the drop or move state, the mask is filled with ' '
 * @return 9x9 char array with 'x', 'o' and ' ',
 */
char **shogi_model_get_available_moves();

/**
 * Returns the hand of player
 * @param is_white true for white player's hand, false for black's
 * @return pointer to array representing amount of pawn in players hand
 */
int *shogi_model_get_hand(gboolean is_white);

/**
 * Returns left time for player
 * @param is_white true for white player, false for black
 * @return left time in miliseconds for player
 */
gint64 shogi_model_timer_get_time(gboolean is_white);

/**
 * Check if specified player is in check
 * @param check_for_black true to check if black is in check
 * @return true if check, false otherwise
 */
gboolean shogi_model_is_check(enum SHOGI_PAWN_DETAILED **board, gboolean check_for_black);

//----------------------------------------------------------------------------------------------------------------------


/**
 * Executed when given field is clicked
 * @param col column on shogi board indexed from 1 to 9
 * @param row row on shogi board indexed from 1 to 9
 * @return true if any change in model happened, false if action was cancelled
 */
gboolean shogi_model_click(int col, int row);

/**
 * Enters into drop mode
 * @param pawn pawn to be dropped
 */
gboolean shogi_model_drop_mode(enum SHOGI_PAWN_DETAILED pawn);

/**
 * Promotes recently moved piece
 * @warning it does not check for correctness, if pawn at selected location is invalid, assert fails
 * @param want_promote true if promotion is accepted, false if declined
 */
void shogi_model_promote(gboolean want_promote);


/**
 * Resets model to initial state
 */
void shogi_model_reset();

/**
 * Creates new empty hitmap
 * @return new hitmap
 */
char **shogi_model_hitmap_new();

/**
 * Generate hitmap for given board and pawn at col and row
 * If there is no pawn at location, hitmap is empty
 * @param board
 * @param col
 * @param row
 * @return
 */
char **shogi_model_hitmap_calc(char **hitmap, enum SHOGI_PAWN_DETAILED **board, int col, int row);

/**
 * Generates hitmap for specified color
 * @param blacks
 * @return
 */
char **shogi_model_hitmap_calc_all(char **hitmap, enum SHOGI_PAWN_DETAILED **board, gboolean blacks);

/**
 * Clears the hitmap
 * @return
 */
void shogi_model_hitmap_clear(char **mask);

/**
 * Restarts timers of both players to initial_time in miliseconds
 * If initial_time is 0, timed mode is disabled
 * @param initial_time initial time for each player
 */
void shogi_model_timer_set(guint32 initial_time);

/**
 * Decreases current players timer by delta seconds
 * @param delta time to be removed from current players timer
 */
void shogi_model_timer_decrease(clock_t delta);

/**
 * Execute to make current player resign
 */
void shogi_model_resign();

//----------------------------------------------------------------------------------------------------------------------


/**
 * Changes current player
 */
inline static void change_player();

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

/**
 * Check if specified player is in checkmate
 * @param black_drops true to check if black is in checkmate
 * @return true if checkmate, false otherwise
 */
static void
shogi_model_exclude_drop_mate(enum SHOGI_PAWN_DETAILED **board, char **hitmap, gboolean black_drops); // DONE


/**
 * Returns hash of given string
 * @param str pointer to string
 * @return hash of string as unsigned long
 */
inline static HASH hash_string(char *str);

/**
 * Serializes the state of game into single string - black_hand|white_hand|board
 * @param model model representing game state to serialize
 */
char *shogi_model_serialize_state();

/**
 * Deserializes state into model. Doesn't import history
 * @param state serialized string representing model state
 */
void shogi_model_deserialize_state(const char *state);

/**
 * Loads game state based on save file
 * @param file save as binary file
 */
void shogi_model_load_game(FILE *file);

/**
 * Parses move of pawn
 * @param pawn enum representing a pawn
 * @param colA starting column, -1 for not needed in notation
 * @param rowA starting row, -1 for not needed in notation
 * @param type 0 for move, 1 for capture, 2 for drop
 * @param colB ending column
 * @param rowB ending row
 * @param promote 0 for no promotion, 1 for promotion 2 for declined promotion
 * @return new char of size 7 representing move
 */
inline static char *
parse_move(enum SHOGI_PAWN_DETAILED pawn, int colA, int rowA, int type, int colB, int rowB, int promote);

/**
 * Appends entry to history.
 * Format in binary is : [move][state_hash][state]
 * with sizes in bytes:
 * move = sizeof(char)*6
 * hash = sizeof(unsigned long)
 * state = sizeof(char) * 96 (7x2 for hands + 81 for board + 1 for null terminator)
 */
inline static void append_history(const char *move, HASH hash, const char *state_serialized);
