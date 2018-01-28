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

void shogi_logger_log(enum SHOGI_LOGGER_LOG_LEVEL level, const char *format, ...);
#endif //SHOGI_LOGGER_H

int shogi_logger_close();