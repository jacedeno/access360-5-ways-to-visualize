// ui.c — navigation shell + shared scaffold + screen-1 setters.
//
// The four screens live in their own files; this file owns the global widget
// table, the Back/Next + swipe navigation, the page-indicator dots, and the
// Broker/Gateway setters. Screens 2-4 setters live in their respective files.
#include "ui_internal.h"

#include <stdio.h>
#include <string.h>

ui_globals_t ui_g;

lv_color_t ui_sev_color(ui_sev_t sev)
{
    switch (sev) {
        case UI_OK:   return UI_COLOR_OK;
        case UI_WARN: return UI_COLOR_WARN;
        case UI_CRIT: return UI_COLOR_CRIT;
        default:      return UI_COLOR_UNKNOWN;
    }
}

// --- navigation -------------------------------------------------------------
static void update_dots(void)
{
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        if (!ui_g.dots[i]) continue;
        lv_obj_set_style_bg_color(ui_g.dots[i],
            i == ui_g.active ? UI_COLOR_TEXT : UI_COLOR_UNKNOWN, 0);
    }
}

void ui_nav_goto(int idx)
{
    if (idx < 0) idx = UI_SCREEN_COUNT - 1;
    if (idx >= UI_SCREEN_COUNT) idx = 0;
    ui_g.active = idx;
    lv_screen_load_anim(ui_g.screens[idx], LV_SCR_LOAD_ANIM_FADE_IN, 150, 0, false);
    // Dots live on each screen footer; refresh the loaded screen's row.
    update_dots();
}

void ui_nav_next(void) { ui_nav_goto(ui_g.active + 1); }
void ui_nav_prev(void) { ui_nav_goto(ui_g.active - 1); }

static void btn_next_cb(lv_event_t *e) { (void)e; ui_nav_next(); }
static void btn_prev_cb(lv_event_t *e) { (void)e; ui_nav_prev(); }

// Swipe gesture on the active screen.
static void gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT)  ui_nav_next();
    else if (dir == LV_DIR_RIGHT) ui_nav_prev();
}

// --- scaffold ---------------------------------------------------------------
lv_obj_t *ui_make_scaffold(const char *title, lv_obj_t **out_screen)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, UI_COLOR_BG, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);

    // Title bar.
    lv_obj_t *title_lbl = lv_label_create(scr);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 12);

    // Content area (between title and footer).
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, BSP_CONTENT_W, BSP_CONTENT_H);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 4, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // Footer: Back | dots | Next.
    lv_obj_t *back = lv_button_create(scr);
    lv_obj_set_size(back, 88, 44);
    lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_obj_set_style_bg_color(back, UI_COLOR_CARD, 0);
    lv_obj_add_event_cb(back, btn_prev_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_lbl);

    lv_obj_t *next = lv_button_create(scr);
    lv_obj_set_size(next, 88, 44);
    lv_obj_align(next, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
    lv_obj_set_style_bg_color(next, UI_COLOR_CARD, 0);
    lv_obj_add_event_cb(next, btn_next_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *next_lbl = lv_label_create(next);
    lv_label_set_text(next_lbl, "Next " LV_SYMBOL_RIGHT);
    lv_obj_center(next_lbl);

    // Page-indicator dots (centered in footer). We store one set of dot handles
    // globally; the last-built screen's dots win, which is fine because dots are
    // only visually meaningful on the active screen.
    lv_obj_t *dotrow = lv_obj_create(scr);
    lv_obj_remove_style_all(dotrow);
    lv_obj_set_size(dotrow, 120, 20);
    lv_obj_align(dotrow, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_flex_flow(dotrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dotrow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        lv_obj_t *d = lv_obj_create(dotrow);
        lv_obj_set_size(d, 10, 10);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(d, 0, 0);
        lv_obj_set_style_margin_left(d, 4, 0);
        lv_obj_set_style_margin_right(d, 4, 0);
        lv_obj_set_style_bg_color(d, UI_COLOR_UNKNOWN, 0);
        ui_g.dots[i] = d;   // last screen built owns the global dot row
    }

    *out_screen = scr;
    return content;
}

// --- public API: create + screen-1 setters ---------------------------------
void ui_create(void)
{
    memset(&ui_g, 0, sizeof(ui_g));
    ui_g.screens[0] = ui_build_screen_broker();
    ui_g.screens[1] = ui_build_screen_sensors();
    ui_g.screens[2] = ui_build_screen_power();
    ui_g.screens[3] = ui_build_screen_sim();
    ui_g.active = 0;
    lv_screen_load(ui_g.screens[0]);
    update_dots();
}

static void set_led(lv_obj_t *led, lv_obj_t *lbl, bool ok,
                    const char *ok_text, const char *bad_text)
{
    if (led) lv_obj_set_style_bg_color(led, ok ? UI_COLOR_OK : UI_COLOR_CRIT, 0);
    if (lbl) lv_label_set_text(lbl, ok ? ok_text : bad_text);
}

void ui_set_broker_connected(bool connected)
{
    set_led(ui_g.broker_led, ui_g.broker_lbl, connected, "Broker: UP", "Broker: DOWN");
}

void ui_set_gateway_online(bool online)
{
    set_led(ui_g.gw_led, ui_g.gw_lbl, online, "Gateway: ONLINE", "Gateway: OFFLINE");
}

void ui_set_messages_per_sec(float mps)
{
    if (!ui_g.mps_lbl) return;
    char buf[40];
    snprintf(buf, sizeof(buf), "Messages/s: %.1f", mps);
    lv_label_set_text(ui_g.mps_lbl, buf);
}

void ui_set_last_error(const char *err)
{
    if (!ui_g.err_lbl) return;
    char buf[128];
    snprintf(buf, sizeof(buf), "Last error: %s",
             (err && err[0]) ? err : "none");
    lv_label_set_text(ui_g.err_lbl, buf);
    lv_obj_set_style_text_color(ui_g.err_lbl,
        (err && err[0]) ? UI_COLOR_WARN : UI_COLOR_TEXT, 0);
}
