//
// Created by Tooster on 22.01.2018.
//

#ifndef CUWR_RESOURCEMANAGER_H
#define CUWR_RESOURCEMANAGER_H


#include "Utils.h"
#include <cairo.h>

/**
 * Loads files to memory
 * @return 0 on success, 1 on error
 */
int shogi_resource_manager_init();

cairo_surface_t *shogi_resource_manager_get_board();
cairo_surface_t *shogi_resource_manager_get_pawn(enum SHOGI_PAWN_DETAILED pawn);

#endif //CUWR_RESOURCEMANAGER_H
