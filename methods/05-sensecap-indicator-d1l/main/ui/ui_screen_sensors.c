// ui_screen_sensors.c — Screen 2: Sensors.
//   - Online count (sensors with now - last_seen < 600 s)
//   - Per-sensor last-seen age, with an online/offline dot
#include "ui_internal.h"

#include <stdio.h>

// One sensor row: [dot] SERIAL (MODEL) ............ AGE
static void make_row(lv_obj_t *parent, int i)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), 36);
    lv_obj_set_style_bg_color(row, UI_COLOR_CARD, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_left(row, 8, 0);
    lv_obj_set_style_pad_right(row, 8, 0);
    lv_obj_set_style_margin_bottom(row, 4, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_set_size(dot, 16, 16);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_bg_color(dot, UI_COLOR_UNKNOWN, 0);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *serial = lv_label_create(row);
    lv_label_set_text(serial, "--");
    lv_obj_set_style_text_color(serial, UI_COLOR_TEXT, 0);
    lv_obj_align(serial, LV_ALIGN_LEFT_MID, 26, 0);

    lv_obj_t *age = lv_label_create(row);
    lv_label_set_text(age, "--");
    lv_obj_set_style_text_color(age, UI_COLOR_TEXT, 0);
    lv_obj_align(age, LV_ALIGN_RIGHT_MID, 0, 0);

    ui_g.sensor_rows[i]   = row;
    ui_g.sensor_dot[i]    = dot;
    ui_g.sensor_serial[i] = serial;
    ui_g.sensor_age[i]    = age;
    lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);   // shown as rows get populated
}

lv_obj_t *ui_build_screen_sensors(void)
{
    lv_obj_t *screen;
    lv_obj_t *c = ui_make_scaffold("Sensors", &screen);

    ui_g.online_lbl = lv_label_create(c);
    lv_label_set_text(ui_g.online_lbl, "Online: --/--");
    lv_obj_set_style_text_color(ui_g.online_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ui_g.online_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_bottom(ui_g.online_lbl, 8, 0);

    for (int i = 0; i < UI_MAX_ROWS; i++) make_row(c, i);

    return screen;
}

// --- setters ----------------------------------------------------------------
void ui_set_online_count(int online, int total)
{
    if (!ui_g.online_lbl) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "Online: %d/%d", online, total);
    lv_label_set_text(ui_g.online_lbl, buf);
    lv_obj_set_style_text_color(ui_g.online_lbl,
        (total > 0 && online == total) ? UI_COLOR_OK :
        (online == 0 ? UI_COLOR_CRIT : UI_COLOR_WARN), 0);
}

void ui_set_sensor_row(int row, const char *serial_str, const char *model,
                       int64_t age_s, bool online)
{
    if (row < 0 || row >= UI_MAX_ROWS) return;
    lv_obj_remove_flag(ui_g.sensor_rows[row], LV_OBJ_FLAG_HIDDEN);

    char buf[48];
    snprintf(buf, sizeof(buf), "%s (%s)", serial_str, model ? model : "?");
    lv_label_set_text(ui_g.sensor_serial[row], buf);

    if (age_s < 0) {
        lv_label_set_text(ui_g.sensor_age[row], "age --");
    } else if (age_s < 600) {
        snprintf(buf, sizeof(buf), "%llds", (long long)age_s);
        lv_label_set_text(ui_g.sensor_age[row], buf);
    } else if (age_s < 3600) {
        snprintf(buf, sizeof(buf), "%lldm", (long long)(age_s / 60));
        lv_label_set_text(ui_g.sensor_age[row], buf);
    } else {
        snprintf(buf, sizeof(buf), "%lldh", (long long)(age_s / 3600));
        lv_label_set_text(ui_g.sensor_age[row], buf);
    }

    // Online dot + age color from the 600 s / 13 h thresholds.
    lv_color_t dotc = online ? UI_COLOR_OK : UI_COLOR_CRIT;
    if (age_s < 0) dotc = UI_COLOR_UNKNOWN;
    lv_obj_set_style_bg_color(ui_g.sensor_dot[row], dotc, 0);

    lv_color_t agec = UI_COLOR_TEXT;
    if (age_s >= 0 && age_s > 46800) agec = UI_COLOR_WARN;   // >13 h => silent
    if (!online && age_s >= 0)       agec = UI_COLOR_CRIT;   // past online cutoff
    lv_obj_set_style_text_color(ui_g.sensor_age[row], agec, 0);
}

void ui_set_sensor_row_count(int count)
{
    for (int i = 0; i < UI_MAX_ROWS; i++) {
        if (i < count) lv_obj_remove_flag(ui_g.sensor_rows[i], LV_OBJ_FLAG_HIDDEN);
        else           lv_obj_add_flag(ui_g.sensor_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
}
