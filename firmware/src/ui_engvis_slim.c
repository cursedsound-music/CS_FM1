/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) COS_SLIM: ui_engvis.c's creatures left out of the build (~30 KB of the FM-1's flash, for cursed-os's own
 * engines and pages). JIANT's interface (HOME + GLO) then draws what it draws without them: vx_draw says it drew nothing
 * (its callers fall back to their graphs), the FM6 and kit pages show their names, HOME's beings their palette's ramp */
static uint8_t vx_small;
static uint16_t vk_heat(int32_t h) { return bg_col(clamp(h, 0, 255)); }
static int vx_draw(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h, int32_t en_home)
{
    (void)t; (void)x; (void)y; (void)w; (void)h; (void)en_home;
    return 0;
}
static void vx_fm6_page(const track_t *t, uint32_t sel, int op2) { (void)t; (void)sel; (void)op2; }
static void vx_fm6env(const track_t *t) { (void)t; }
static void vx_kit_page(const track_t *t, int32_t lane)
{
    (void)lane;
    cv_text(8, 6 - AF_X_CAP_Y, &AF_X, "KIT", T_THEME);
    cv_text(8, 20, &AF_X, trk_engp(t)->name, T_MID);
}
