
#ifndef LV_DEMO_MUSIC_H
#define LV_DEMO_MUSIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"



#define LV_DEMO_MUSIC_HANDLE_SIZE  20


/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void gui_music(void);
void lv_demo_music_close(void);

const char * _lv_demo_music_get_title(uint32_t track_id);
const char * _lv_demo_music_get_artist(uint32_t track_id);
const char * _lv_demo_music_get_genre(uint32_t track_id);
uint32_t _lv_demo_music_get_track_length(uint32_t track_id);

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
