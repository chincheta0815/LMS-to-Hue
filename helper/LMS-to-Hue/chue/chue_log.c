/*
 *  chue - philips hue library for C
 *
 *  (c) Adrian Smith 2012-2015, triode1@btinternet.com
 *  (c) Philippe 2016, philippe_44@outlook.com
 *  (c) Rouven Weiler 2017
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "chue_log.h"

// logging functions
const char *chue_log_time(void) {
    static char buf[100];
#if WIN
    SYSTEMTIME lt;
    GetLocalTime(&lt);
    sprintf(buf, "[%02d:%02d:%02d.%03d]", lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    strftime(buf, sizeof(buf), "[%T.", localtime(&tv.tv_sec));
    sprintf(buf+strlen(buf), "%06ld]", (long)tv.tv_usec);
#endif
    return buf;
}

/*---------------------------------------------------------------------------*/
void chue_log_print(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fflush(stderr);
}

/*---------------------------------------------------------------------------*/
chue_loglevel_t chue_log_debug2level(char *level)
{
    if (!strcmp(level, "error")) return lCHUE_LOG_ERROR;
    if (!strcmp(level, "warn")) return lCHUE_LOG_WARN;
    if (!strcmp(level, "info")) return lCHUE_LOG_INFO;
    if (!strcmp(level, "debug")) return lCHUE_LOG_DEBUG;
    if (!strcmp(level, "sdebug")) return lCHUE_LOG_SDEBUG;
    return lCHUE_LOG_WARN;
}

/*---------------------------------------------------------------------------*/
char *chue_log_level2debug(chue_loglevel_t level)
{
    switch (level) {
    case lCHUE_LOG_ERROR: return "error";
    case lCHUE_LOG_WARN: return "warn";
    case lCHUE_LOG_INFO: return "info";
    case lCHUE_LOG_DEBUG: return "debug";
    case lCHUE_LOG_SDEBUG: return "sdebug";
    default: return "warn";
    }
}
