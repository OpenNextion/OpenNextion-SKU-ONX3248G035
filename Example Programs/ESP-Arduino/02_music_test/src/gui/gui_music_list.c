/**
 * @file lv_demo_music_list.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "gui_music_list.h"
#include "gui_music_main.h"
#include "gui_hal.h"

/* Incremental list fill + background track length calculation
 *
 * Why: Creating the whole list at boot blocks the LVGL task (SD IO + object creation),
 * delaying the main screen animations.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

/*********************
 *      DEFINES
 *********************/

/* UI responsiveness tuning */
#define LIST_FILL_PERIOD_MS            (20U)   /* LVGL timer period */
#define LIST_FILL_BATCH                (6U)    /* Buttons created per tick */

/* Background duration calculation */
#define LEN_TASK_STACK_BYTES           (8192U)
#define LEN_TASK_PRIO                  (tskIDLE_PRIORITY + 1)
#define LEN_QUEUE_LEN                  (16U)
#define LEN_UI_POLL_PERIOD_MS          (50U)

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    uint32_t track_id;
} len_req_t;

typedef struct {
    uint32_t track_id;
    uint32_t seconds;
    char artist[64];
} len_rsp_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t * add_list_btn(lv_obj_t * parent, uint32_t track_id);
static void btn_click_event_cb(lv_event_t * e);

static void list_fill_timer_cb(lv_timer_t * t);
static void len_ui_poll_timer_cb(lv_timer_t * t);
static void len_task(void * arg);
static void request_track_length(uint32_t track_id);
static void format_time_str(uint32_t sec, char * out, size_t out_len);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t * list;
static const lv_font_t * font_small;
static const lv_font_t * font_medium;
static lv_style_t style_scrollbar;
static lv_style_t style_btn;
static lv_style_t style_btn_pr;
static lv_style_t style_btn_chk;
static lv_style_t style_btn_dis;
static lv_style_t style_title;
static lv_style_t style_artist;
static lv_style_t style_time;
LV_IMG_DECLARE(img_lv_demo_music_btn_list_play);
LV_IMG_DECLARE(img_lv_demo_music_btn_list_pause);

LV_FONT_DECLARE(cn_font_16);
LV_FONT_DECLARE(cn_font_12);

/* Incremental list build state */
static lv_timer_t * s_list_fill_timer;
static uint32_t s_track_cnt;
static uint32_t s_next_track_id;

/* Checked item tracking (main screen may call btn_check before the item exists) */
static bool s_checked_valid;
static uint32_t s_checked_track_id;

/* Background duration calc */
static QueueHandle_t s_len_req_q;
static QueueHandle_t s_len_rsp_q;
static TaskHandle_t s_len_task;
static lv_timer_t * s_len_ui_poll_timer;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * _lv_demo_music_list_create(lv_obj_t * parent)
{
#if LV_DEMO_MUSIC_LARGE
    font_small = &lv_font_montserrat_16;
    font_medium = &lv_font_montserrat_22;
#else
    font_small =  &cn_font_12;
    font_medium = &cn_font_16;
#endif

    lv_style_init(&style_scrollbar);
    lv_style_set_width(&style_scrollbar,  4);
    lv_style_set_bg_opa(&style_scrollbar, LV_OPA_COVER);
    lv_style_set_bg_color(&style_scrollbar, lv_color_hex3(0xeee));
    lv_style_set_radius(&style_scrollbar, LV_RADIUS_CIRCLE);
    lv_style_set_pad_right(&style_scrollbar, 4);

    static const lv_coord_t grid_cols[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
#if LV_DEMO_MUSIC_LARGE
    static const lv_coord_t grid_rows[] = {35,  30, LV_GRID_TEMPLATE_LAST};
#else
    static const lv_coord_t grid_rows[] = {22,  17, LV_GRID_TEMPLATE_LAST};
#endif
    lv_style_init(&style_btn);
    lv_style_set_bg_opa(&style_btn, LV_OPA_TRANSP);
    lv_style_set_grid_column_dsc_array(&style_btn, grid_cols);
    lv_style_set_grid_row_dsc_array(&style_btn, grid_rows);
    lv_style_set_grid_row_align(&style_btn, LV_GRID_ALIGN_CENTER);
    lv_style_set_layout(&style_btn, LV_LAYOUT_GRID);
#if LV_DEMO_MUSIC_LARGE
    lv_style_set_pad_right(&style_btn, 30);
#else
    lv_style_set_pad_right(&style_btn, 20);
#endif
    lv_style_init(&style_btn_pr);
    lv_style_set_bg_opa(&style_btn_pr, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn_pr,  lv_color_hex(0x4c4965));

    lv_style_init(&style_btn_chk);
    lv_style_set_bg_opa(&style_btn_chk, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn_chk, lv_color_hex(0x4c4965));

    lv_style_init(&style_btn_dis);
    lv_style_set_text_opa(&style_btn_dis, LV_OPA_40);
    lv_style_set_img_opa(&style_btn_dis, LV_OPA_40);

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, font_medium);
    lv_style_set_text_color(&style_title, lv_color_hex(0xffffff));

    lv_style_init(&style_artist);
    lv_style_set_text_font(&style_artist, font_small);
    lv_style_set_text_color(&style_artist, lv_color_hex(0xb1b0be));

    lv_style_init(&style_time);
    lv_style_set_text_font(&style_time, font_medium);
    lv_style_set_text_color(&style_time, lv_color_hex(0xffffff));

    /*Create an empty transparent container*/
    list = lv_obj_create(parent);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, LV_HOR_RES, LV_VER_RES - LV_DEMO_MUSIC_HANDLE_SIZE);
    lv_obj_set_y(list, LV_DEMO_MUSIC_HANDLE_SIZE);
    lv_obj_add_style(list, &style_scrollbar, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scroll_snap_x(list, LV_SCROLL_SNAP_NONE);

    /* Create list items incrementally to avoid blocking the LVGL task */
    s_track_cnt = gui_hal_get_track_count();
    s_next_track_id = 0;

    /* Init checked state tracking */
    s_checked_valid = true;
    s_checked_track_id = 0;

    /* Background duration calculator (optional; list shows "--:--" until ready) */
    if (s_track_cnt > 0) {
        if (!s_len_req_q) {
            s_len_req_q = xQueueCreate(LEN_QUEUE_LEN, sizeof(len_req_t));
        }
        if (!s_len_rsp_q) {
            s_len_rsp_q = xQueueCreate(LEN_QUEUE_LEN, sizeof(len_rsp_t));
        }
        if (!s_len_task && s_len_req_q && s_len_rsp_q) {
            (void)xTaskCreate(len_task,
                              "music_len",
                              LEN_TASK_STACK_BYTES / sizeof(StackType_t),
                              NULL,
                              LEN_TASK_PRIO,
                              &s_len_task);
        }
        if (!s_len_ui_poll_timer) {
            s_len_ui_poll_timer = lv_timer_create(len_ui_poll_timer_cb, LEN_UI_POLL_PERIOD_MS, NULL);
        }
    }

    if (!s_list_fill_timer) {
        s_list_fill_timer = lv_timer_create(list_fill_timer_cb, LIST_FILL_PERIOD_MS, NULL);
    }

#if LV_DEMO_MUSIC_SQUARE || LV_DEMO_MUSIC_ROUND
    lv_obj_set_scroll_snap_y(list, LV_SCROLL_SNAP_CENTER);
#endif

    /* Defer actual UI state until items are created */
    _lv_demo_music_list_btn_check(0, true);

    return list;
}

void _lv_demo_music_list_close(void)
{
    /* Stop incremental builders */
    if (s_list_fill_timer) {
        lv_timer_del(s_list_fill_timer);
        s_list_fill_timer = NULL;
    }
    if (s_len_ui_poll_timer) {
        lv_timer_del(s_len_ui_poll_timer);
        s_len_ui_poll_timer = NULL;
    }

    /* Stop background length task */
    if (s_len_task) {
        vTaskDelete(s_len_task);
        s_len_task = NULL;
    }
    if (s_len_req_q) {
        vQueueDelete(s_len_req_q);
        s_len_req_q = NULL;
    }
    if (s_len_rsp_q) {
        vQueueDelete(s_len_rsp_q);
        s_len_rsp_q = NULL;
    }

    lv_style_reset(&style_scrollbar);
    lv_style_reset(&style_btn);
    lv_style_reset(&style_btn_pr);
    lv_style_reset(&style_btn_chk);
    lv_style_reset(&style_btn_dis);
    lv_style_reset(&style_title);
    lv_style_reset(&style_artist);
    lv_style_reset(&style_time);
}

void _lv_demo_music_list_btn_check(uint32_t track_id, bool state)
{
    /* Remember the desired checked track even if the list item isn't created yet */
    if (state) {
        s_checked_valid = true;
        s_checked_track_id = track_id;
    }
    else if (s_checked_valid && s_checked_track_id == track_id) {
        s_checked_valid = false;
    }

    if (!list) return;
    lv_obj_t * btn = lv_obj_get_child(list, track_id);
    if (!btn) return;
    lv_obj_t * icon = lv_obj_get_child(btn, 0);
    if (!icon) return;

    if(state) {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        lv_img_set_src(icon, &img_lv_demo_music_btn_list_pause);
        lv_obj_scroll_to_view(btn, LV_ANIM_ON);
    }
    else {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
        lv_img_set_src(icon, &img_lv_demo_music_btn_list_play);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_obj_t * add_list_btn(lv_obj_t * parent, uint32_t track_id)
{
    /* NOTE: Do NOT calculate duration here. It requires SD IO and would block the LVGL task.
     * The duration will be calculated in a background task and the UI will update later.
     */
    char time[8];
    strncpy(time, "--:--", sizeof(time));
    const char * title = _lv_demo_music_get_title(track_id);
    const char * artist = _lv_demo_music_get_artist(track_id);
    lv_obj_t * btn = lv_obj_create(parent);

    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(btn, LV_DIR_NONE);

    lv_obj_remove_style_all(btn);
#if LV_DEMO_MUSIC_LARGE
    lv_obj_set_size(btn, lv_pct(100), 110);
#else
    lv_obj_set_size(btn, lv_pct(100), 60);
#endif

    lv_obj_add_style(btn, &style_btn, 0);
    lv_obj_add_style(btn, &style_btn_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &style_btn_chk, LV_STATE_CHECKED);
    lv_obj_add_style(btn, &style_btn_dis, LV_STATE_DISABLED);
    lv_obj_add_event_cb(btn, btn_click_event_cb, LV_EVENT_CLICKED, NULL);


    lv_obj_t * icon = lv_img_create(btn);
    lv_img_set_src(icon, &img_lv_demo_music_btn_list_play);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 2);

    lv_obj_t * title_label = lv_label_create(btn);
    lv_label_set_text(title_label, title);
    lv_obj_set_grid_cell(title_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_style(title_label, &style_title, 0);

    lv_obj_t * artist_label = lv_label_create(btn);
    lv_label_set_text(artist_label, artist);
    lv_obj_add_style(artist_label, &style_artist, 0);
    lv_obj_set_grid_cell(artist_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    lv_obj_t * time_label = lv_label_create(btn);
    lv_label_set_text(time_label, time);
    lv_obj_add_style(time_label, &style_time, 0);
    lv_obj_set_grid_cell(time_label, LV_GRID_ALIGN_END, 2, 1, LV_GRID_ALIGN_CENTER, 0, 2);

    /* Apply checked state if requested before this item existed */
    if (s_checked_valid && (track_id == s_checked_track_id)) {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        lv_img_set_src(icon, &img_lv_demo_music_btn_list_pause);
    }

    /* Request duration calculation in background */
    request_track_length(track_id);

    LV_IMG_DECLARE(img_lv_demo_music_list_border);
    lv_obj_t * border = lv_img_create(btn);
    lv_img_set_src(border, &img_lv_demo_music_list_border);
    lv_obj_set_width(border, lv_pct(120));
    lv_obj_align(border, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(border, LV_OBJ_FLAG_IGNORE_LAYOUT);

    return btn;
}

static void btn_click_event_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);

    uint32_t idx = lv_obj_get_child_id(btn);

    gui_hal_play_index(idx);
    
    _lv_demo_music_play(idx);
}

static void list_fill_timer_cb(lv_timer_t * t)
{
    LV_UNUSED(t);

    if (!list) return;

    if (s_track_cnt == 0) {
        if (s_list_fill_timer) {
            lv_timer_del(s_list_fill_timer);
            s_list_fill_timer = NULL;
        }
        return;
    }

    uint32_t created = 0;
    while ((created < LIST_FILL_BATCH) && (s_next_track_id < s_track_cnt)) {
        add_list_btn(list, s_next_track_id);
        s_next_track_id++;
        created++;
    }

    /* Done */
    if (s_next_track_id >= s_track_cnt) {
        if (s_list_fill_timer) {
            lv_timer_del(s_list_fill_timer);
            s_list_fill_timer = NULL;
        }

        /* Ensure the currently checked track (if any) is visible once the list is ready */
        if (s_checked_valid) {
            _lv_demo_music_list_btn_check(s_checked_track_id, true);
        }
    }
}

static void request_track_length(uint32_t track_id)
{
    if (!s_len_req_q || !s_len_rsp_q) return;
    if (track_id >= s_track_cnt) return;

    len_req_t req = { .track_id = track_id };
    (void)xQueueSend(s_len_req_q, &req, 0);
}

static void format_time_str(uint32_t sec, char * out, size_t out_len)
{
    if (!out || out_len == 0) return;
    /* Keep a minimum of 0:00 */
    lv_snprintf(out, out_len, "%"LV_PRIu32":%02"LV_PRIu32, sec / 60U, sec % 60U);
}

static void len_ui_poll_timer_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    if (!list || !s_len_rsp_q) return;

    /* Avoid spending too long in a single LVGL tick */
    for (uint32_t i = 0; i < 8; i++) {
        len_rsp_t rsp;
        if (xQueueReceive(s_len_rsp_q, &rsp, 0) != pdTRUE) {
            break;
        }

        lv_obj_t * btn = lv_obj_get_child(list, rsp.track_id);
        if (!btn) continue;

        /* Child order: icon(0), title(1), artist(2), time(3), border(4) */
        lv_obj_t * time_label = lv_obj_get_child(btn, 3);
        if (!time_label) continue;

        char buf[16];
        format_time_str(rsp.seconds, buf, sizeof(buf));
        lv_label_set_text(time_label, buf);
        /* Update artist label (child order: icon(0), title(1), artist(2), time(3), border(4)) */
        lv_obj_t * artist_label = lv_obj_get_child(btn, 2);
        if (artist_label) {
            if (rsp.artist[0] == '\0') {
                lv_label_set_text(artist_label, "Unknown artist");
            } else {
                lv_label_set_text(artist_label, rsp.artist);
            }
        }
    }
}

static void len_task(void * arg)
{
    LV_UNUSED(arg);

    len_req_t req;
    while (1) {
        if (!s_len_req_q || !s_len_rsp_q) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (xQueueReceive(s_len_req_q, &req, portMAX_DELAY) == pdTRUE) {
            uint32_t sec = 0;
            if (req.track_id < gui_hal_get_track_count()) {
                sec = _lv_demo_music_get_track_length(req.track_id);
            }
            char artist_buf[64];
            artist_buf[0] = '\0';
            /* Read ID3 Artist in background to avoid blocking the LVGL task */
            (void)gui_hal_get_artist_album_by_index((size_t)req.track_id,
                                                    artist_buf, sizeof(artist_buf),
                                                    NULL, 0);

            len_rsp_t rsp = {0};
            rsp.track_id = req.track_id;
            rsp.seconds = sec;

            if (artist_buf[0] == '\0') {
                strncpy(rsp.artist, "Unknown artist", sizeof(rsp.artist) - 1);
            } else {
                strncpy(rsp.artist, artist_buf, sizeof(rsp.artist) - 1);
            }
            rsp.artist[sizeof(rsp.artist) - 1] = '\0';
            (void)xQueueSend(s_len_rsp_q, &rsp, 0);
        }
    }
}
