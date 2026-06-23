// ui_screen_sim.c — Screen 4: 4G / SIM (Hologram).
//   - Data used vs limit (%) with a progress bar (amber > 80 %)
//   - SIM live/paused (paused + usage => critical)
//   - Last-connect age + poll freshness (poll stale > 30 min => warning)
//
// All values arrive from the .150 Hologram poller on access360/<gw>/sim/usage.
// When no SIM message has been received, the screen shows a "no data" state.
#include "ui_internal.h"

#include <stdio.h>

lv_obj_t *ui_build_screen_sim(void)
{
    lv_obj_t *screen;
    lv_obj_t *c = ui_make_scaffold("4G / SIM", &screen);

    ui_g.sim_pct_lbl = lv_label_create(c);
    lv_label_set_text(ui_g.sim_pct_lbl, "Data used: -- %");
    lv_obj_set_style_text_color(ui_g.sim_pct_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ui_g.sim_pct_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_bottom(ui_g.sim_pct_lbl, 6, 0);

    ui_g.sim_bar = lv_bar_create(c);
    lv_obj_set_size(ui_g.sim_bar, lv_pct(100), 22);
    lv_bar_set_range(ui_g.sim_bar, 0, 100);
    lv_bar_set_value(ui_g.sim_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui_g.sim_bar, UI_COLOR_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_g.sim_bar, UI_COLOR_OK, LV_PART_INDICATOR);
    lv_obj_set_style_margin_bottom(ui_g.sim_bar, 12, 0);

    ui_g.sim_used_lbl = lv_label_create(c);
    lv_label_set_text(ui_g.sim_used_lbl, "-- / -- MB");
    lv_obj_set_style_text_color(ui_g.sim_used_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_margin_bottom(ui_g.sim_used_lbl, 6, 0);

    ui_g.sim_state_lbl = lv_label_create(c);
    lv_label_set_text(ui_g.sim_state_lbl, "SIM: --");
    lv_obj_set_style_text_color(ui_g.sim_state_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ui_g.sim_state_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_margin_bottom(ui_g.sim_state_lbl, 6, 0);

    ui_g.sim_conn_lbl = lv_label_create(c);
    lv_label_set_text(ui_g.sim_conn_lbl, "Last connect: --");
    lv_obj_set_style_text_color(ui_g.sim_conn_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_margin_bottom(ui_g.sim_conn_lbl, 6, 0);

    ui_g.sim_poll_lbl = lv_label_create(c);
    lv_label_set_text(ui_g.sim_poll_lbl, "Poll age: --");
    lv_obj_set_style_text_color(ui_g.sim_poll_lbl, UI_COLOR_UNKNOWN, 0);

    return screen;
}

static void age_text(char *out, size_t n, const char *prefix, int64_t age_s)
{
    if (age_s < 0)        snprintf(out, n, "%s --", prefix);
    else if (age_s < 60)  snprintf(out, n, "%s %llds", prefix, (long long)age_s);
    else if (age_s < 3600)snprintf(out, n, "%s %lldm", prefix, (long long)(age_s / 60));
    else                  snprintf(out, n, "%s %lldh", prefix, (long long)(age_s / 3600));
}

void ui_set_sim(bool have_sim, int used_mb, int limit_mb, int pct,
                bool paused, int64_t last_connect_age_s, int64_t poll_age_s)
{
    char buf[64];

    if (!have_sim) {
        lv_label_set_text(ui_g.sim_pct_lbl, "Data used: no data");
        lv_label_set_text(ui_g.sim_used_lbl, "(waiting for .150 poller)");
        lv_label_set_text(ui_g.sim_state_lbl, "SIM: unknown");
        lv_label_set_text(ui_g.sim_conn_lbl, "Last connect: --");
        lv_label_set_text(ui_g.sim_poll_lbl, "Poll age: --");
        lv_bar_set_value(ui_g.sim_bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_text_color(ui_g.sim_state_lbl, UI_COLOR_UNKNOWN, 0);
        return;
    }

    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    snprintf(buf, sizeof(buf), "Data used: %d %%", pct);
    lv_label_set_text(ui_g.sim_pct_lbl, buf);

    lv_bar_set_value(ui_g.sim_bar, pct, LV_ANIM_OFF);
    // > 80 % of cap => WARNING.
    lv_color_t barc = (pct >= 80) ? UI_COLOR_WARN : UI_COLOR_OK;
    lv_obj_set_style_bg_color(ui_g.sim_bar, barc, LV_PART_INDICATOR);

    snprintf(buf, sizeof(buf), "%d / %d MB", used_mb, limit_mb);
    lv_label_set_text(ui_g.sim_used_lbl, buf);

    // paused + usage => CRITICAL; live => OK.
    bool crit = paused && used_mb > 0;
    lv_label_set_text(ui_g.sim_state_lbl, paused ? "SIM: PAUSED" : "SIM: LIVE");
    lv_obj_set_style_text_color(ui_g.sim_state_lbl,
        crit ? UI_COLOR_CRIT : (paused ? UI_COLOR_WARN : UI_COLOR_OK), 0);

    age_text(buf, sizeof(buf), "Last connect:", last_connect_age_s);
    lv_label_set_text(ui_g.sim_conn_lbl, buf);

    age_text(buf, sizeof(buf), "Poll age:", poll_age_s);
    lv_label_set_text(ui_g.sim_poll_lbl, buf);
    // Poll stale > 30 min => WARNING.
    lv_obj_set_style_text_color(ui_g.sim_poll_lbl,
        (poll_age_s >= 0 && poll_age_s > 1800) ? UI_COLOR_WARN : UI_COLOR_UNKNOWN, 0);
}
