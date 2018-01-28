//
// Created by Tooster on 22.01.2018.
//

#include "ResourceManager.h"
#include "Logger.h"
#include <cairo.h>
#include <stdlib.h>
#include <math.h>

static cairo_surface_t *texture_board;
static cairo_surface_t *texture_pawn[SHOGI_PAWN_DETAILED_TYPE_COUNT];

int shogi_resource_manager_init() {
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Loading resources...");
    texture_board = cairo_image_surface_create_from_png("resources/board.png");

    // @formatter:off
    texture_pawn[SHOGI_PAWN_K_WHITE]        = cairo_image_surface_create_from_png("resources/pawns/96x96/K_white.png");
    texture_pawn[SHOGI_PAWN_K_BLACK]        = cairo_image_surface_create_from_png("resources/pawns/96x96/K_black.png");

    texture_pawn[SHOGI_PAWN_G_WHITE]        = cairo_image_surface_create_from_png("resources/pawns/96x96/G_white.png");
    texture_pawn[SHOGI_PAWN_G_BLACK]        = cairo_image_surface_create_from_png("resources/pawns/96x96/G_black.png");

    texture_pawn[SHOGI_PAWN_S_WHITE]        = cairo_image_surface_create_from_png("resources/pawns/96x96/S_white.png");
    texture_pawn[SHOGI_PAWN_S_BLACK]        = cairo_image_surface_create_from_png("resources/pawns/96x96/S_black.png");
    texture_pawn[SHOGI_PAWN_S_PRO_WHITE]    = cairo_image_surface_create_from_png("resources/pawns/96x96/S+_white.png");
    texture_pawn[SHOGI_PAWN_S_PRO_BLACK]    = cairo_image_surface_create_from_png("resources/pawns/96x96/S+_black.png");

    texture_pawn[SHOGI_PAWN_N_WHITE]        = cairo_image_surface_create_from_png("resources/pawns/96x96/N_white.png");
    texture_pawn[SHOGI_PAWN_N_BLACK]        = cairo_image_surface_create_from_png("resources/pawns/96x96/N_black.png");
    texture_pawn[SHOGI_PAWN_N_PRO_WHITE]    = cairo_image_surface_create_from_png("resources/pawns/96x96/N+_white.png");
    texture_pawn[SHOGI_PAWN_N_PRO_BLACK]    = cairo_image_surface_create_from_png("resources/pawns/96x96/N+_black.png");

    texture_pawn[SHOGI_PAWN_L_WHITE]        = cairo_image_surface_create_from_png("resources/pawns/96x96/L_white.png");
    texture_pawn[SHOGI_PAWN_L_BLACK]        = cairo_image_surface_create_from_png("resources/pawns/96x96/L_black.png");
    texture_pawn[SHOGI_PAWN_L_PRO_WHITE]    = cairo_image_surface_create_from_png("resources/pawns/96x96/L+_white.png");
    texture_pawn[SHOGI_PAWN_L_PRO_BLACK]    = cairo_image_surface_create_from_png("resources/pawns/96x96/L+_black.png");

    texture_pawn[SHOGI_PAWN_B_WHITE]        = cairo_image_surface_create_from_png("resources/pawns/96x96/B_white.png");
    texture_pawn[SHOGI_PAWN_B_BLACK]        = cairo_image_surface_create_from_png("resources/pawns/96x96/B_black.png");
    texture_pawn[SHOGI_PAWN_B_PRO_WHITE]    = cairo_image_surface_create_from_png("resources/pawns/96x96/B+_white.png");
    texture_pawn[SHOGI_PAWN_B_PRO_BLACK]    = cairo_image_surface_create_from_png("resources/pawns/96x96/B+_black.png");

    texture_pawn[SHOGI_PAWN_R_WHITE]        = cairo_image_surface_create_from_png("resources/pawns/96x96/R_white.png");
    texture_pawn[SHOGI_PAWN_R_BLACK]        = cairo_image_surface_create_from_png("resources/pawns/96x96/R_black.png");
    texture_pawn[SHOGI_PAWN_R_PRO_WHITE]    = cairo_image_surface_create_from_png("resources/pawns/96x96/R+_white.png");
    texture_pawn[SHOGI_PAWN_R_PRO_BLACK]    = cairo_image_surface_create_from_png("resources/pawns/96x96/R+_black.png");

    texture_pawn[SHOGI_PAWN_P_WHITE]        = cairo_image_surface_create_from_png("resources/pawns/96x96/P_white.png");
    texture_pawn[SHOGI_PAWN_P_BLACK]        = cairo_image_surface_create_from_png("resources/pawns/96x96/P_black.png");
    texture_pawn[SHOGI_PAWN_P_PRO_WHITE]    = cairo_image_surface_create_from_png("resources/pawns/96x96/P+_white.png");
    texture_pawn[SHOGI_PAWN_P_PRO_BLACK]    = cairo_image_surface_create_from_png("resources/pawns/96x96/P+_black.png");
    // @formatter:on

    int load_status = cairo_surface_status(texture_board);
    if (cairo_surface_status(texture_board) != 0) {
        shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_FATAL, "Couldn't load texture board.");
        return load_status;
    }
    /*
    load_status == CAIRO_STATUS_NO_MEMORY ||
    load_status == CAIRO_STATUS_FILE_NOT_FOUND ||
    load_status == CAIRO_STATUS_READ_ERROR
    */
    for (int i = 0; i < SHOGI_PAWN_DETAILED_TYPE_COUNT; ++i) {
        load_status = cairo_surface_status(texture_pawn[i]);
        if (load_status != 0) {
            shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_FATAL, "Couldn't load texture for pawn enum number %d.", i);
            return load_status;
        }
    }

    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Textures loaded.");
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Rotating black textures with cairo...");
    for (int i = 0; i < SHOGI_PAWN_DETAILED_TYPE_COUNT; ++i) {
        if (SHOGI_PAWN_IS_BLACK(i)) {

            cairo_surface_t *rotated = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 96,96);
            cairo_t *cr = cairo_create(rotated);
            cairo_translate(cr, 96 / 2, 96 / 2);
            cairo_rotate(cr, M_PI);
            cairo_translate(cr, -96 / 2, -96 / 2);
            cairo_set_source_surface(cr, texture_pawn[i], 0, 0);
            cairo_paint(cr);

            cairo_destroy(cr);
            cr = cairo_create(texture_pawn[i]);
            cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
            cairo_paint(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            cairo_set_source_surface(cr, rotated, 0, 0);
            cairo_paint(cr);

            cairo_destroy(cr);
            cairo_surface_destroy(rotated);
        }
    }
    shogi_logger_log(SHOGI_LOGGER_LOG_LEVEL_DEBUG, "Black textures rotated.");

    return 0;
}


cairo_surface_t *shogi_resource_manager_get_pawn(enum SHOGI_PAWN_DETAILED pawn) {
    if (pawn < 0 || pawn > SHOGI_PAWN_DETAILED_TYPE_COUNT) return NULL;
    return texture_pawn[pawn];
}

cairo_surface_t *shogi_resource_manager_get_board() {
    return texture_board;
}
