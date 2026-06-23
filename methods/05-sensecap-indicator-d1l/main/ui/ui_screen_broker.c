// ui_screen_broker.c — Screen 1: Broker / Gateway.
//   - Broker connected LED/badge (esp-mqtt connection state)
//   - Gateway online LED/badge (ap/notify + will)
//   - Messages/s (sliding window over access360/#)
//   - Last error (error/notify "Error")
#include "ui_internal.h"

// A labeled status card with a circular LED on the left and a text label.
static void make_status_card(lv_obj_t *parent, lv_obj_t **out_led,
                             lv_obj_t **out_lbl, const char *initial)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, lv_pct(100), 64);
    lv_obj_set_style_bg_color(card, UI_COLOR_CARD, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_margin_bottom(card, 8, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *led = lv_obj_create(card);
    lv_obj_set_size(led, 26, 26);
    lv_obj_set_style_radius(led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(led, 0, 0);
    lv_obj_set_style_bg_color(led, UI_COLOR_UNKNOWN, 0);
    lv_obj_align(led, LV_ALIGN_LEFT_MID, 2, 0);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, initial);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 44, 0);

    *out_led = led;
    *out_lbl = lbl;
}

lv_obj_t *ui_build_screen_broker(void)
{
    lv_obj_t *screen;
    lv_obj_t *c = ui_make_scaffold("Broker / Gateway", &screen);

    make_status_card(c, &ui_g.broker_led, &ui_g.broker_lbl, "Broker: ...");
    make_status_card(c, &ui_g.gw_led, &ui_g.gw_lbl, "Gateway: ...");

    // Messages/s line.
    ui_g.mps_lbl = lv_label_create(c);
    lv_label_set_text(ui_g.mps_lbl, "Messages/s: --");
    lv_obj_set_style_text_color(ui_g.mps_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ui_g.mps_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_margin_top(ui_g.mps_lbl, 6, 0);

    // Last error line (wraps).
    ui_g.err_lbl = lv_label_create(c);
    lv_label_set_long_mode(ui_g.err_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(ui_g.err_lbl, lv_pct(100));
    lv_label_set_text(ui_g.err_lbl, "Last error: none");
    lv_obj_set_style_text_color(ui_g.err_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_margin_top(ui_g.err_lbl, 6, 0);

    return screen;
}
