#ifndef ATV_CORE_H
#define ATV_CORE_H
#include <stddef.h>
#include <wchar.h>
#include "datv.h"
#ifdef __cplusplus
extern "C" {
#endif
#define CONFIG_FILENAME "atv-contestnummer.conf"
typedef struct {
    char call[97], locator[49], code[17]; /* UTF-8 */
    int mode, automatic, aspect, resolution, band;
    int show, inverse, blue_yellow, show_sum, top_code, genius, ebu_top, ebu_bottom;
    DatvSettings ts;
    DatvUdpSettings udp;
} AppConfig;
AppConfig config_defaults(void);
/* UTF-8 absolute path. Load: 1 success, 0 absent, -1 invalid/unreadable.
 * Failure leaves the caller's settings and any existing saved file intact. */
int config_load(const char *path, AppConfig *out);
int config_save(const char *path, const AppConfig *settings);
typedef struct { int width, height; } Resolution;
#define LOCATOR_MAX_LENGTH 12
#define RESOLUTION_COUNT 11
#define DEFAULT_RESOLUTION_INDEX 3 /* 320x240 (4:3), 320x180 (16:9). */
extern const Resolution resolutions43[RESOLUTION_COUNT], resolutions169[RESOLUTION_COUNT];
extern const wchar_t *const bands[12];
extern const wchar_t *const band_files[12];
typedef struct { unsigned char red, green, blue; } ContestColor;
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
