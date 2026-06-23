// ui_screen_power.c — Screen 3: Power & Signal.
//   - Per-sensor battery %  (red < 20 %)
//   - Per-sensor RSSI (dBm) (red < -75 dBm)
#include "ui_internal.h"

#include <stdio.h>

// Row: SERIAL (MODEL) .... BATT% .... RSSI dBm
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

    lv_obj_t *serial = lv_label_create(row);
    lv_label_set_text(serial, "--");
    lv_obj_set_style_text_color(serial, UI_COLOR_TEXT, 0);
    lv_obj_align(serial, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *batt = lv_label_create(row);
    lv_label_set_text(batt, "-- %");
    lv_obj_set_style_text_color(batt, UI_COLOR_TEXT, 0);
    lv_obj_align(batt, LV_ALIGN_CENTER, 36, 0);

    lv_obj_t *rssi = lv_label_create(row);
    lv_label_set_text(rssi, "-- dBm");
    lv_obj_set_style_text_color(rssi, UI_COLOR_TEXT, 0);
    lv_obj_align(rssi, LV_ALIGN_RIGHT_MID, 0, 0);

    ui_g.power_rows[i]   = row;
    ui_g.power_serial[i] = serial;
    ui_g.power_batt[i]   = batt;
    ui_g.power_rssi[i]   = rssi;
    lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *ui_build_screen_power(void)
{
    lv_obj_t *screen;
    lv_obj_t *c = ui_make_scaffold("Power & Signal", &screen);

    // Column header.
    lv_obj_t *hdr = lv_label_create(c);
    lv_label_set_text(hdr, "Sensor            Batt        RSSI");
    lv_obj_set_style_text_color(hdr, UI_COLOR_UNKNOWN, 0);
    lv_obj_set_style_margin_bottom(hdr, 6, 0);

    for (int i = 0; i < UI_MAX_ROWS; i++) make_row(c, i);

    return screen;
}

// --- setters ----------------------------------------------------------------
void ui_set_power_row(int row, const char *serial_str, const char *model,
                      bool have_batt, int batt_pct,
                      bool have_rssi, int rssi_dbm)
{
    if (row < 0 || row >= UI_MAX_ROWS) return;
    lv_obj_remove_flag(ui_g.power_rows[row], LV_OBJ_FLAG_HIDDEN);

    char buf[48];
    snprintf(buf, sizeof(buf), "%s (%s)", serial_str, model ? model : "?");
    lv_label_set_text(ui_g.power_serial[row], buf);

    if (have_batt) {
        snprintf(buf, sizeof(buf), "%d%%", batt_pct);
        lv_label_set_text(ui_g.power_batt[row], buf);
        // Red < 20 %, amber < 40 %, else green.
        lv_color_t c = (batt_pct < 20) ? UI_COLOR_CRIT :
                       (batt_pct < 40) ? UI_COLOR_WARN : UI_COLOR_OK;
        lv_obj_set_style_text_color(ui_g.power_batt[row], c, 0);
    } else {
        lv_label_set_text(ui_g.power_batt[row], "--");
        lv_obj_set_style_text_color(ui_g.power_batt[row], UI_COLOR_UNKNOWN, 0);
    }

    if (have_rssi) {
        snprintf(buf, sizeof(buf), "%d dBm", rssi_dbm);
        lv_label_set_text(ui_g.power_rssi[row], buf);
        // Red < -75 dBm (CTC practical drop point), amber < -65, else green.
        lv_color_t c = (rssi_dbm < -75) ? UI_COLOR_CRIT :
                       (rssi_dbm < -65) ? UI_COLOR_WARN : UI_COLOR_OK;
        lv_obj_set_style_text_color(ui_g.power_rssi[row], c, 0);
    } else {
        lv_label_set_text(ui_g.power_rssi[row], "--");
        lv_obj_set_style_text_color(ui_g.power_rssi[row], UI_COLOR_UNKNOWN, 0);
    }
}

void ui_set_power_row_count(int count)
{
    for (int i = 0; i < UI_MAX_ROWS; i++) {
        if (i < count) lv_obj_remove_flag(ui_g.power_rows[i], LV_OBJ_FLAG_HIDDEN);
        else           lv_obj_add_flag(ui_g.power_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
}
