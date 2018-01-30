//
// Created by Tooster on 26.01.2018.
//

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include "Logger.h"

FILE *log_file;
bool initialized = false;
static char *log_level_strings[] = {
        "FATAL",
        "ERROR",
        "WARN ",
        "INFO ",
        "DEBUG"
};


int shogi_logger_init() {
    log_file = fopen("Shogi.log", "a"); // open for appending
    if (log_file == NULL) {
        printf("Cannot open Shogi.log for appending\n");
        return 1;
    }
    initialized = true;
    return 0;
}

void shogi_logger_log(enum SHOGI_LOGGER_LOG_LEVEL level, const char *fmt, ...) {
    if (level > SHOGI_LOGGER_MAX_LOG_LEVEL)
        return;

    if (!initialized)
        if (shogi_logger_init() != 0) // if cannot initialize logger, abort app
            abort();

    char prefix[32];
    char date[20];
    struct tm *sTm;
    time_t now = time(0);
    sTm = gmtime(&now);
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", sTm);
    sprintf(prefix, "[%s] [%s]", date, log_level_strings[level]);

    va_list args;
    va_start(args, fmt);

    fprintf(log_file, "%s : ", prefix);
    vfprintf(log_file, fmt, args);
    fprintf(log_file, "\n");

    va_end(args);

    fflush(log_file);
}

int shogi_logger_close() {
    return fclose(log_file);
}


