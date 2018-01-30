//
// Created by Tooster on 26.01.2018.
//

#ifndef SHOGI_LOGGER_H
#define SHOGI_LOGGER_H

#include <stdlib.h>
#include <stdbool.h>

#define SHOGI_LOGGER_MAX_LOG_LEVEL SHOGI_LOGGER_LOG_LEVEL_DEBUG

enum SHOGI_LOGGER_LOG_LEVEL{
    SHOGI_LOGGER_LOG_LEVEL_FATAL,
    SHOGI_LOGGER_LOG_LEVEL_ERROR,
    SHOGI_LOGGER_LOG_LEVEL_WARN,
    SHOGI_LOGGER_LOG_LEVEL_INFO,
    SHOGI_LOGGER_LOG_LEVEL_DEBUG
};

static int shogi_logger_init();

/**
 * Writes message with format ant arguments to log file with given level.
 * Time is set accordingly to as UTC+00:00
 * @param level logging level
 * @param fmt format as in printf
 * @param ... argument list as in printf
 */
void shogi_logger_log(enum SHOGI_LOGGER_LOG_LEVEL level, const char *fmt, ...);

/**
 * Closes file
 * @warning Should be run once at the end of programs life
 * @return same as return of fclose()
 */
int shogi_logger_close();
#endif //SHOGI_LOGGER_H
