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

#ifndef __CHUE_LOG_H
#define __CHUE_LOG_H

typedef enum {
    lCHUE_LOG_SILENCE = 0,
    lCHUE_LOG_ERROR,
    lCHUE_LOG_WARN,
    lCHUE_LOG_INFO,
    lCHUE_LOG_DEBUG,
    lCHUE_LOG_SDEBUG
} chue_loglevel_t;

extern chue_loglevel_t chue_loglevel;
//static chue_loglevel_t *clp = &chue_loglevel;

const char *chue_log_time(void);
void chue_log_print(const char *fmt, ...);
chue_loglevel_t chue_log_debug2level(char *level);
char *chue_log_level2debug(chue_loglevel_t level);

#define CHUE_ERROR(fmt, ...) if (*clp >= lCHUE_LOG_ERROR) chue_log_print("%s %s:%d " fmt "\n", chue_log_time(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define CHUE_WARN(fmt, ...) if (*clp >= lCHUE_LOG_WARN) chue_log_print("%s %s:%d " fmt "\n", chue_log_time(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define CHUE_INFO(fmt, ...) if (*clp >= lCHUE_LOG_INFO) chue_log_print("%s %s:%d " fmt "\n", chue_log_time(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define CHUE_DEBUG(fmt, ...) if (*clp >= lCHUE_LOG_DEBUG) chue_log_print("%s %s:%d " fmt "\n", chue_log_time(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define CHUE_SDEBUG(fmt, ...) if (*clp >= lCHUE_LOG_SDEBUG) chue_log_print("%s %s:%d " fmt "\n", chue_log_time(), __FUNCTION__, __LINE__, ##__VA_ARGS__)

#endif
