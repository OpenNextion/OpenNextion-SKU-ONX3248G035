
#ifndef LV_DEMO_MUSIC_LIST_H
#define LV_DEMO_MUSIC_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "gui_music.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
lv_obj_t * _lv_demo_music_list_create(lv_obj_t * parent);
void _lv_demo_music_list_close(void);

void _lv_demo_music_list_btn_check(uint32_t track_id, bool state);

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_DEMO_MUSIC_LIST_H*/
