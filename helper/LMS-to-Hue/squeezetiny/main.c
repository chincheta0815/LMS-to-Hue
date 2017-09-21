/*
 *  Squeezelite - lightweight headless squeezebox emulator
 *
 *  (c) Adrian Smith 2012-2015, triode1@btinternet.com
 *  (c) Philippe, philippe_44@outlook.com for raop/multi-instance modifications
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


#include "squeezelite.h"

#include <math.h>
#include <signal.h>
#include <ctype.h>

#define LOCK_S   mutex_lock(ctx->streambuf->mutex)
#define UNLOCK_S mutex_unlock(ctx->streambuf->mutex)
#if 0
#define LOCK_O   mutex_lock(ctx->outputbuf->mutex)
#define UNLOCK_O mutex_unlock(ctx->outputbuf->mutex)
#else
#define LOCK_O
#define UNLOCK_O
#endif
#define LOCK_D   mutex_lock(ctx->decode.mutex)
#define UNLOCK_D mutex_unlock(ctx->decode.mutex)
#define LOCK_P   mutex_lock(ctx->mutex)
#define UNLOCK_P mutex_unlock(ctx->mutex)

struct thread_ctx_s thread_ctx[MAX_PLAYER];


/*----------------------------------------------------------------------------*/
/* locals */
/*----------------------------------------------------------------------------*/
static void sq_wipe_device(struct thread_ctx_s *ctx);

extern log_level	slimmain_loglevel;
static log_level	*loglevel = &slimmain_loglevel;

/*---------------------------------------------------------------------------*/
void sq_end() {
    int i;

    for (i = 0; i < MAX_PLAYER; i++) {
        if (thread_ctx[i].in_use) {
            sq_wipe_device(&thread_ctx[i]);
        }
    }

    decode_end();
#if WIN
    winsock_close();
#endif
}


/*--------------------------------------------------------------------------*/
void sq_wipe_device(struct thread_ctx_s *ctx) {
    ctx->callback = NULL;
    ctx->in_use = false;

    slimproto_close(ctx);
    output_close(ctx);
#if RESAMPLE
    process_end(ctx);
#endif
    decode_close(ctx);
    stream_close(ctx);
}


/*--------------------------------------------------------------------------*/
void sq_delete_device(sq_dev_handle_t handle) {
    struct thread_ctx_s *ctx;

    if (handle) {
        ctx = &thread_ctx[handle - 1];
        sq_wipe_device(ctx);
    }
}


/*---------------------------------------------------------------------------*/
static char from_hex(char ch) {

    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/*---------------------------------------------------------------------------*/
static char to_hex(char code) {
    static char hex[] = "0123456789abcdef";

    return hex[code & 15];
}


/*---------------------------------------------------------------------------*/
/* IMPORTANT: be sure to free() the returned string after use */
static char *cli_encode(char *str) {
    char *pstr = str, *buf = malloc(strlen(str) * 3 + 1), *pbuf = buf;
    while (*pstr) {
        if ( isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' ||
                               *pstr == '~' || *pstr == ' ' || *pstr == ')' ||
                               *pstr == '(' )
            *pbuf++ = *pstr;
        else if (*pstr == '%') {
            *pbuf++ = '%',*pbuf++ = '2', *pbuf++ = '5';
        }
        else {
            *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
        }
        pstr++;
    }
    *pbuf = '\0';

    return buf;
}


/*---------------------------------------------------------------------------*/
/* IMPORTANT: be sure to free() the returned string after use */
static char *cli_decode(char *str) {
    char *pstr = str, *buf = malloc(strlen(str) + 1), *pbuf = buf;
    while (*pstr) {
        if (*pstr == '%') {
            if (pstr[1] && pstr[2]) {
                *pbuf++ = (from_hex(pstr[1]) << 4) | from_hex(pstr[2]);
                pstr += 2;
            }
        }
        else {
            *pbuf++ = *pstr;
        }
        pstr++;
    }
    *pbuf = '\0';

    return buf;
}


/*---------------------------------------------------------------------------*/
/* IMPORTANT: be sure to free() the returned string after use */
static char *cli_find_tag(char *str, char *tag) {
    char *p, *res = NULL;
    char *buf = malloc(max(strlen(str), strlen(tag)) + 4);

    strcpy(buf, tag);
    strcat(buf, "%3a");
    if ((p = stristr(str, buf)) != NULL) {
        int i = 0;
        p += strlen(buf);
        while (*(p+i) != ' ' && *(p+i) != '\n' && *(p+i)) i++;
        if (i) {
            strncpy(buf, p, i);
            buf[i] = '\0';
            res = url_decode(buf);
        }
    }
    free(buf);

    return res;
}


/*---------------------------------------------------------------------------*/
bool cli_open_socket(struct thread_ctx_s *ctx) {
    struct sockaddr_in addr;

    if (ctx->cli_sock > 0) return true;

    ctx->cli_sock = socket(AF_INET, SOCK_STREAM, 0);
    set_nonblock(ctx->cli_sock);
    set_nosigpipe(ctx->cli_sock);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ctx->slimproto_ip;
    addr.sin_port = htons(9090);

    if (connect(ctx->cli_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
#if !WIN
        if (last_error() && last_error() != EINPROGRESS) {
#else
        if (last_error() != WSAEWOULDBLOCK) {
#endif
            LOG_ERROR("[%p] unable to connect to server with cli", ctx);
            ctx->cli_sock = -1;

            return false;
        }
    }
    LOG_INFO("[%p]: opened CLI socket %d", ctx, ctx->cli_sock);

    return true;
}


/*---------------------------------------------------------------------------*/
char *cli_send_cmd(char *cmd, bool req, bool decode, struct thread_ctx_s *ctx) {

#define CLI_LEN 2048

    char packet[CLI_LEN];
    int wait = 100;
    size_t len;
	char *rsp = NULL;

    mutex_lock(ctx->cli_mutex);

    if (!cli_open_socket(ctx)) {
        mutex_unlock(ctx->cli_mutex);
        
        return NULL;
    }

    ctx->cli_timestamp = gettime_ms();
	cmd = cli_encode(cmd);

    if (req) len = sprintf(packet, "%s ?\n", cmd);
    else len = sprintf(packet, "%s\n", cmd);

    send_packet((u8_t*) packet, len, ctx->cli_sock);
    LOG_SDEBUG("[%p]: cmd %s", ctx, packet);

    // first receive the tag and then point to the last '\n'
    len = 0;
    while (wait--)	{
        int k;
        usleep(10000);
        k = recv(ctx->cli_sock, packet + len, CLI_LEN-1 - len, 0);
        if (k < 0) {
            if (last_error() == ERROR_WOULDBLOCK) continue;
            else break;
        }
        len += k;
        packet[len] = '\0';
        if (strchr(packet, '\n') && stristr(packet, cmd)) {
            rsp = packet;
            break;
        }
    }

    if (!wait) {
        LOG_WARN("[%p]: Timeout waiting for CLI reponse (%s)", ctx, cmd);
    }

    LOG_SDEBUG("[%p]: rsp %s", ctx, rsp);

    if (rsp && ((rsp = stristr(rsp, cmd)) != NULL)) {
        rsp += strlen(cmd);
        while (*rsp && *rsp == ' ') rsp++;

        if (decode) rsp = cli_decode(rsp);
        else rsp = strdup(rsp);
        *(strrchr(rsp, '\n')) = '\0';
    }

    NFREE(cmd);
    mutex_unlock(ctx->cli_mutex);

    return rsp;
}


/*--------------------------------------------------------------------------*/
static void sq_init_metadata(sq_metadata_t *metadata) {
    metadata->artist 	= NULL;
    metadata->album 	= NULL;
    metadata->title 	= NULL;
    metadata->genre 	= NULL;
    metadata->path 		= NULL;
    metadata->artwork 	= NULL;

    metadata->track 	= 0;
    metadata->index 	= 0;
    metadata->file_size = 0;
    metadata->duration 	= 0;
    metadata->remote 	= false;
}


/*--------------------------------------------------------------------------*/
void sq_default_metadata(sq_metadata_t *metadata, bool init) {
    if (init) sq_init_metadata(metadata);

    if (!metadata->title) metadata->title 	= strdup("[LMS to RAOP]");
    if (!metadata->album) metadata->album 	= strdup("[no album]");
    if (!metadata->artist) metadata->artist = strdup("[no artist]");
    if (!metadata->genre) metadata->genre 	= strdup("[no genre]");
    /*
    if (!metadata->path) metadata->path = strdup("[no path]");
    if (!metadata->artwork) metadata->artwork = strdup("[no artwork]");
    */
}


/*--------------------------------------------------------------------------*/
sq_action_t sq_get_mode(sq_dev_handle_t handle) {
    struct thread_ctx_s *ctx = &thread_ctx[handle - 1];
    char cmd[1024];
    char *rsp;

    if (!handle || !ctx->in_use) {
        LOG_ERROR("[%p]: no handle %d", ctx, handle);

        return true;
    }

    sprintf(cmd, "%s mode", ctx->cli_id);
    rsp  = cli_send_cmd(cmd, true, false, ctx);

    if (!rsp) return SQ_NONE;
    if (!strcasecmp(rsp, "play")) return SQ_PLAY;
    if (!strcasecmp(rsp, "pause")) return SQ_PAUSE;
    if (!strcasecmp(rsp, "stop")) return SQ_STOP;

    return SQ_NONE;
}


/*--------------------------------------------------------------------------*/
bool sq_get_metadata(sq_dev_handle_t handle, sq_metadata_t *metadata, bool next) {
    struct thread_ctx_s *ctx = &thread_ctx[handle - 1];
    char cmd[1024];
    char *rsp, *p;
    u16_t idx;

    if (!handle || !ctx->in_use) {
        LOG_ERROR("[%p]: no handle %d", ctx, handle);
        sq_default_metadata(metadata, true);

        return false;
    }

    sprintf(cmd, "%s playlist index", ctx->cli_id);
    rsp = cli_send_cmd(cmd, true, true, ctx);

    if (!rsp || !*rsp) {
        LOG_ERROR("[%p]: missing index", ctx);
        NFREE(rsp);
        sq_default_metadata(metadata, true);

        return false;
    }

    sq_init_metadata(metadata);

    idx = atol(rsp);
    NFREE(rsp);
    metadata->index = idx;

    if (next) {
        sprintf(cmd, "%s playlist tracks", ctx->cli_id);
        rsp = cli_send_cmd(cmd, true, true, ctx);
        if (rsp && atol(rsp)) idx = (idx + 1) % atol(rsp);
        else idx = 0;
        NFREE(rsp);
    }

    sprintf(cmd, "%s playlist remote %d", ctx->cli_id, idx);
    rsp  = cli_send_cmd(cmd, true, true, ctx);
    if (rsp && *rsp == '1') metadata->remote = true;
    else metadata->remote = false;
    NFREE(rsp)

    sprintf(cmd, "%s playlist path %d", ctx->cli_id, idx);
    rsp = cli_send_cmd(cmd, true, true, ctx);
    if (rsp && *rsp) {
        metadata->path = rsp;
        metadata->track_hash = hash32(metadata->path);
        sprintf(cmd, "%s songinfo 0 10 url:%s tags:cfldatgrK", ctx->cli_id, metadata->path);
        rsp = cli_send_cmd(cmd, false, false, ctx);
    }

    if (rsp && *rsp) {
        metadata->title = cli_find_tag(rsp, "title");
        metadata->artist = cli_find_tag(rsp, "artist");
        metadata->album = cli_find_tag(rsp, "album");
        metadata->genre = cli_find_tag(rsp, "genre");

        if ((p = cli_find_tag(rsp, "duration")) != NULL) {
            metadata->duration = 1000 * atof(p);
            free(p);
        }

        if ((p = cli_find_tag(rsp, "filesize")) != NULL) {
            metadata->file_size = atol(p);
            /*
            at this point, LMS sends the original filesize, not the transcoded
            so it simply does not work
            */
            metadata->file_size = 0;
            free(p);
        }

        if ((p = cli_find_tag(rsp, "tracknum")) != NULL) {
            metadata->track = atol(p);
            free(p);
        }

        metadata->artwork = cli_find_tag(rsp, "artwork_url");
        if (!metadata->artwork || !strlen(metadata->artwork)) {
            NFREE(metadata->artwork);
            if ((p = cli_find_tag(rsp, "coverid")) != NULL) {
                metadata->artwork = malloc(SQ_STR_LENGTH);
                snprintf(metadata->artwork, SQ_STR_LENGTH, "http://%s:%s/music/%s/cover.jpg", ctx->server_ip, ctx->server_port, p);
                free(p);
            }
        }
    }
    else {
        LOG_INFO("[%p]: no metadata using songinfo", ctx, idx);
        NFREE(rsp);

        sprintf(cmd, "%s playlist title %d", ctx->cli_id, idx);
        metadata->title = cli_send_cmd(cmd, true, true, ctx);

        sprintf(cmd, "%s playlist album %d", ctx->cli_id, idx);
        metadata->album = cli_send_cmd(cmd, true, true, ctx);

        sprintf(cmd, "%s playlist artist %d", ctx->cli_id, idx);
        metadata->artist = cli_send_cmd(cmd, true, true, ctx);

        sprintf(cmd, "%s playlist genre %d", ctx->cli_id, idx);
        metadata->genre = cli_send_cmd(cmd, true, true, ctx);

        sprintf(cmd, "%s status %d 1 tags:K", ctx->cli_id, idx);
        rsp = cli_send_cmd(cmd, false, false, ctx);
        if (rsp && *rsp) metadata->artwork = cli_find_tag(rsp, "artwork_url");
        NFREE(rsp);

        sprintf(cmd, "%s playlist duration %d", ctx->cli_id, idx);
        rsp = cli_send_cmd(cmd, true, true, ctx);
        if (rsp) metadata->duration = 1000 * atof(rsp);
    }
    NFREE(rsp);

    if (metadata->artwork && !strncmp(metadata->artwork, "/imageproxy/", strlen("/imageproxy/"))) {
        char *artwork = malloc(SQ_STR_LENGTH);

        snprintf(artwork, SQ_STR_LENGTH, "http://%s:%s%s", ctx->server_ip, ctx->server_port, metadata->artwork);
        free(metadata->artwork);
        metadata->artwork = artwork;
    }

    sq_default_metadata(metadata, false);

    LOG_DEBUG("[%p]: idx %d\n\tartist:%s\n\talbum:%s\n\ttitle:%s\n"
              "\tgenre:%s\n\tduration:%d.%03d\n\tsize:%d\n\tcover:%s",
              ctx, metadata->index, metadata->artist,
              metadata->album, metadata->title, metadata->genre,
              div(metadata->duration, 1000).quot,
              div(metadata->duration,1000).rem, metadata->file_size,
              metadata->artwork ? metadata->artwork : "");

    return true;
}


/*--------------------------------------------------------------------------*/
void sq_free_metadata(sq_metadata_t *metadata) {
    NFREE(metadata->artist);
    NFREE(metadata->album);
    NFREE(metadata->title);
    NFREE(metadata->genre);
    NFREE(metadata->path);
    NFREE(metadata->artwork);
}


/*--------------------------------------------------------------------------*/
u32_t sq_get_time(sq_dev_handle_t handle) {
    struct thread_ctx_s *ctx = &thread_ctx[handle - 1];
    char cmd[128];
    char *rsp;
    u32_t time = 0;

    if (!handle || !ctx->in_use) {
        LOG_ERROR("[%p]: no handle or CLI socket %d", ctx, handle);

        return 0;
    }

    sprintf(cmd, "%s time", ctx->cli_id);
    rsp = cli_send_cmd(cmd, true, true, ctx);
    if (rsp && *rsp) {
        time = (u32_t) (atof(rsp) * 1000);
    }
    else {
        LOG_ERROR("[%p] cannot gettime", ctx);
    }

    NFREE(rsp);

    return time;
}


/*--------------------------------------------------------------------------*/
void sq_notify(sq_dev_handle_t handle, void *caller_id, sq_event_t event, u8_t *cookie, void *param) {
    struct thread_ctx_s *ctx = &thread_ctx[handle - 1];
    char cmd[128] = "", *rsp;

    LOG_SDEBUG("[%p] notif %d", ctx, event);

    // squeezelite device has not started yet or is off ...
    if (!ctx->running || !ctx->on || !handle || !ctx->in_use) return;

    switch (event) {
        case SQ_PLAY_PAUSE: {
            sprintf(cmd, "%s pause", ctx->cli_id);
            break;
        }
        case SQ_PLAY: {
            sprintf(cmd, "%s play", ctx->cli_id);
            break;
        }
        case SQ_PAUSE: {
            sprintf(cmd, "%s mode", ctx->cli_id);
            rsp = cli_send_cmd(cmd, true, true, ctx);
            if (rsp && *rsp)
                sprintf(cmd, "%s pause %d", ctx->cli_id, strstr(rsp, "pause") ? 0 : 1);
            else
                sprintf(cmd, "%s pause", ctx->cli_id);
            break;
        }
        case SQ_STOP: {
            sprintf(cmd, "%s stop", ctx->cli_id);
            break;
        }
        case SQ_VOLUME: {
            if (strstr(param, "up")) sprintf(cmd, "%s mixer volume +5", ctx->cli_id);
            else if (strstr(param, "down")) sprintf(cmd, "%s mixer volume +5", ctx->cli_id);
            else sprintf(cmd, "%s mixer volume %s", ctx->cli_id, (char*) param);
            break;
        }
        case SQ_MUTE_TOGGLE: {
            sprintf(cmd, "%s mixer muting toggle", ctx->cli_id);
            break;
        }
        case SQ_PREVIOUS: {
            sprintf(cmd, "%s playlist index -1", ctx->cli_id);
            break;
        }
        case SQ_NEXT: {
            sprintf(cmd, "%s playlist index +1", ctx->cli_id);
            break;
        }
        case SQ_SHUFFLE: {
            sprintf(cmd, "%s playlist shuffle 1", ctx->cli_id);
            break;
        }
        case SQ_FF_REW: {
            sprintf(cmd, "%s time %+d", ctx->cli_id, *((s16_t*) param));
            break;
        }

        default:
            break;
    }

    if (*cmd) {
        rsp = cli_send_cmd(cmd, false, true, ctx);
        NFREE(rsp);
    }
}


/*---------------------------------------------------------------------------*/
void sq_init(void) {
#if WIN
    winsock_init();
#endif

    decode_init();
}


/*---------------------------------------------------------------------------*/
void sq_release_device(sq_dev_handle_t handle) {
    if (handle) thread_ctx[handle - 1].in_use = false;
}


/*---------------------------------------------------------------------------*/
sq_dev_handle_t sq_reserve_device(void *MR, sq_callback_t callback) {
    int ctx_i;
    struct thread_ctx_s *ctx;

    /* find a free thread context - this must be called in a LOCKED context */
    for (ctx_i = 0; ctx_i < MAX_PLAYER; ctx_i++)
        if (!thread_ctx[ctx_i].in_use) break;

    if (ctx_i < MAX_PLAYER) {
        // this sets a LOT of data to proper defaults (NULL, false ...)
        memset(&thread_ctx[ctx_i], 0, sizeof(struct thread_ctx_s));
        thread_ctx[ctx_i].in_use = true;
    }
    else return false;

    ctx = thread_ctx + ctx_i;
    ctx->self = ctx_i + 1;
    ctx->on = false;
    ctx->callback = callback;
    ctx->MR = MR;

    return ctx_i + 1;
}


/*---------------------------------------------------------------------------*/
bool sq_run_device(void *vplayer, sq_dev_handle_t handle, hue_bridge_t *hue, sq_dev_param_t *param) {
    struct thread_ctx_s *ctx = &thread_ctx[handle - 1];

    memcpy(&ctx->config, param, sizeof(sq_dev_param_t));

    sprintf(ctx->cli_id, "%02x:%02x:%02x:%02x:%02x:%02x",
                         ctx->config.mac[0], ctx->config.mac[1], ctx->config.mac[2],
                         ctx->config.mac[3], ctx->config.mac[4], ctx->config.mac[5]);

    stream_thread_init(ctx->config.stream_buf_size, ctx);
    output_hue_thread_init(vplayer, hue, ctx->config.output_buf_size, ctx);
    decode_thread_init(ctx);

#if RESAMPLE
    if (param->resample) {
        process_init(param->resample_options, ctx);
    }
#endif

    slimproto_thread_init(ctx);

    return true;
}

