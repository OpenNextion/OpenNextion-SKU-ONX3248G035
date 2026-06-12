/**
 * @file gui_music.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "gui_music.h"
#include "gui_music_main.h"
#include "gui_music_list.h"

#include "gui_hal.h" 


#include <string.h>
#include <stdint.h>


/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
#if DEMO_MUSIC_AUTO_PLAY
    static void auto_step_cb(lv_timer_t * timer);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t * ctrl;
static lv_obj_t * list;


#if DEMO_MUSIC_AUTO_PLAY
    static lv_timer_t * auto_step_timer;
#endif

static lv_color_t original_screen_bg_color;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void gui_music(void)
{
    original_screen_bg_color = lv_obj_get_style_bg_color(lv_scr_act(), 0);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x343247), 0);

    list = _lv_demo_music_list_create(lv_scr_act());
    ctrl = _lv_demo_music_main_create(lv_scr_act());

#if DEMO_MUSIC_AUTO_PLAY
    auto_step_timer = lv_timer_create(auto_step_cb, 1000, NULL);
#endif
}

void lv_demo_music_close(void)
{
    /* Delete all animations */
    lv_anim_del(NULL, NULL);

#if DEMO_MUSIC_AUTO_PLAY
    lv_timer_del(auto_step_timer);
#endif
    _lv_demo_music_list_close();
    _lv_demo_music_main_close();

    lv_obj_clean(lv_scr_act());

    lv_obj_set_style_bg_color(lv_scr_act(), original_screen_bg_color, 0);
}

const char * _lv_demo_music_get_title(uint32_t track_id)
{
    /* Return NULL if track_id is out of range. */
    if (track_id >= gui_hal_get_track_count()) {
        return NULL;
    }

    static char title_buf[64];

    if (!gui_hal_get_title_by_index(track_id, title_buf, sizeof(title_buf))) {
        return NULL;
    }

    return title_buf;
}
/* -------------------------------------------------------------------------- */
/* Track metadata (Artist/Album)                                              */
/* - Artist: ID3v2(TPE1) / ID3v1(artist) fallback -> "Unknown artist"          */
/* - Album : ID3v2(TALB) / ID3v1(album)  fallback -> "Unknown"                 */
/* Notes: This function may read from SD card. Avoid calling it in list build. */
/* -------------------------------------------------------------------------- */
static uint32_t s_meta_cache_id = UINT32_MAX;
static char s_meta_artist[64];
static char s_meta_album[64];

static void s_meta_load(uint32_t track_id)
{
    if (track_id == s_meta_cache_id) return;

    s_meta_cache_id = track_id;
    s_meta_artist[0] = '\0';
    s_meta_album[0]  = '\0';

    (void)gui_hal_get_artist_album_by_index((size_t)track_id,
                                            s_meta_artist, sizeof(s_meta_artist),
                                            s_meta_album,  sizeof(s_meta_album));                             
    if (s_meta_artist[0] == '\0') {
        strncpy(s_meta_artist, "Unknown artist", sizeof(s_meta_artist) - 1);
        s_meta_artist[sizeof(s_meta_artist) - 1] = '\0';
    }
    if (s_meta_album[0] == '\0') {
        strncpy(s_meta_album, "Unknown", sizeof(s_meta_album) - 1);
        s_meta_album[sizeof(s_meta_album) - 1] = '\0';
    }
    printf("track_id = %ld s_meta_artist = %s len = %d , s_meta_album = %s  len = %d\r\n",track_id,s_meta_artist,strlen(s_meta_artist),s_meta_album,strlen(s_meta_album));   
}

const char * _lv_demo_music_get_artist(uint32_t track_id)
{
    s_meta_load(track_id);
    return s_meta_artist;
}

/* Reuse the "genre" UI field to show Album name */
const char * _lv_demo_music_get_genre(uint32_t track_id)
{
    s_meta_load(track_id);
    return s_meta_album;
}

uint32_t _lv_demo_music_get_track_length(uint32_t track_id)
{
   return gui_hal_get_track_length(track_id);
}

