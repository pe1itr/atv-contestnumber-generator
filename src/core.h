#ifndef ATV_CORE_H
#define ATV_CORE_H
#include <stddef.h>
#include <wchar.h>
#include "datv.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { IMAGE_CONTEST, IMAGE_PM5544, IMAGE_FUBK, IMAGE_PM5644, IMAGE_MODE_COUNT };
extern const wchar_t *const image_modes[IMAGE_MODE_COUNT];
extern const int image_mode_order[IMAGE_MODE_COUNT];
extern const wchar_t contest_color_advice[];
int image_mode_row(int mode);
int valid_fubk_locator(const wchar_t *s);
#define CONFIG_FILENAME "atv-contestnummer.conf"
enum { DVB_S, DVB_S2, DVB_T, DVB_SYSTEM_COUNT };
#define DVB_SYMBOL_RATE_COUNT 12 /* Persistent IDs, including retired slots. */
#define DVB_SYMBOL_RATE_CHOICE_COUNT 10
#define DVB_BANDWIDTH_COUNT 6
#define DVB_FEC_COUNT 10
#define DVB_GUARD_COUNT 3
typedef struct {
    int system, symbol_rate, fec, pilots, guard, bandwidth_khz;
} DvbSettings;
extern const wchar_t *const dvb_system_names[DVB_SYSTEM_COUNT];
extern const int dvb_symbol_rates[DVB_SYMBOL_RATE_COUNT];
extern const int dvb_symbol_rate_order[DVB_SYMBOL_RATE_CHOICE_COUNT];
int dvb_symbol_rate_row(int id);
extern const int dvb_bandwidths[DVB_BANDWIDTH_COUNT];
extern const wchar_t *const dvb_fec_names[DVB_FEC_COUNT];
extern const wchar_t *const dvb_guard_names[DVB_GUARD_COUNT];
DvbSettings dvb_defaults(void);
DvbSettings motion_dvb_defaults(void);
DatvSettings motion_defaults(void);
/* Bottom 40 rows, limited-range luma. The other rows remain untouched. */
void motion_luma(unsigned char *y, unsigned frame);
int motion_options(int count, const char *const *values, DatvSettings *settings,
                   wchar_t call[25], wchar_t code[5], wchar_t locator[13]);
/* QPSK, S2 normal FECFRAME, T 2K non-hierarchical. Complete 188-byte TS
 * capacity, rounded down to whole bit/s; zero means invalid parameters. */
int dvb_bitrate(DvbSettings s);
int dvb_audio_allowed(DvbSettings s);
/* Selectable persistent FEC IDs, strongest correction first. */
int dvb_fec_choices(DvbSettings s, int ids[DVB_FEC_COUNT]);
#define DATV_MIN_BITRATE 30080
typedef struct {
    char call[97], locator[49], code[17]; /* UTF-8 */
    int mode, automatic, aspect, resolution, band;
    int show, inverse, blue_yellow, show_sum, top_code, genius, ebu_top, ebu_bottom;
    DatvSettings ts;
    DatvUdpSettings udp;
    StationInfo station;
    TeletextSettings teletext;
    DvbSettings ts_dvb, udp_dvb;
} AppConfig;
AppConfig config_defaults(void);
/* UTF-8 absolute path. Load: 1 success, 0 absent, -1 invalid/unreadable.
 * Failure leaves the caller's settings and any existing saved file intact. */
int config_load(const char *path, AppConfig *out);
int config_save(const char *path, const AppConfig *settings);
typedef struct { int width, height; } Resolution;
#define LOCATOR_MAX_LENGTH 12
#define RESOLUTION_COUNT 13
#define DEFAULT_RESOLUTION_INDEX 5 /* 320x240 (4:3), 320x180 (16:9). */
extern const Resolution resolutions43[RESOLUTION_COUNT], resolutions169[RESOLUTION_COUNT];
extern const wchar_t *const bands[12];
extern const wchar_t *const band_files[12];
typedef struct { unsigned char red, green, blue; } ContestColor;
/* Level 1 editor. Colour/mode controls occupy a cell; rows reset to white text. */
typedef struct { unsigned char raw[TELETEXT_CELLS]; int cursor, grid; } TeletextEditor;
typedef struct { int foreground, background, mosaic; unsigned char code, blocks; } TeletextCell;
extern const char *const teletext_color_names[7];
extern const unsigned teletext_colors[8];
void teletext_editor_load(TeletextEditor *e, const TeletextSettings *s);
void teletext_editor_save(const TeletextEditor *e, TeletextSettings *s);
void teletext_editor_example(TeletextEditor *e);
TeletextCell teletext_editor_cell(const TeletextEditor *e, int index);
const char *teletext_editor_type(TeletextEditor *e, unsigned c);
const char *teletext_editor_paste(TeletextEditor *e, const char *text);
/* Insert selected colour, new background and restored ink/mode in three blank cells. */
const char *teletext_editor_background(TeletextEditor *e, int color);
void teletext_editor_control(TeletextEditor *e, unsigned c);
void teletext_editor_click(TeletextEditor *e, int x, int y, int width, int height, int erase);
enum { TT_LEFT, TT_RIGHT, TT_UP, TT_DOWN, TT_HOME, TT_END, TT_ENTER, TT_BACKSPACE, TT_DELETE };
void teletext_editor_key(TeletextEditor *e, int key);
typedef void (*TeletextRect)(void *, int, int, int, int, unsigned);
typedef void (*TeletextGlyph)(void *, int, int, int, int, char, unsigned);
void teletext_editor_plot(const TeletextEditor *e, int width, int height,
                          TeletextRect rect, TeletextGlyph glyph, void *context);
typedef struct { ContestColor background, foreground; } ContestPalette;
#define EBU_BAR_COUNT 8
#define EBU_STRIP_PERCENT 9
extern const ContestColor ebu_colors[EBU_BAR_COUNT];
ContestPalette contest_palette(int blue_yellow, int inverse);
const wchar_t *contest_color_suffix(int blue_yellow);
int valid_call(const wchar_t *s);
int valid_locator(const wchar_t *s);
void filename_locator(wchar_t out[7], const wchar_t *locator);
int locator_square_changed(wchar_t previous[7], const wchar_t *locator);
int valid_code(const wchar_t *s);
int code_digit_sum(const wchar_t *s);
int generated_code_valid(unsigned code);
void filename_call(wchar_t *out, const wchar_t *in);
#ifdef __cplusplus
}
#endif
#endif
