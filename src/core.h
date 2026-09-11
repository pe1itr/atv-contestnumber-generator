#ifndef ATV_CORE_H
#define ATV_CORE_H
#include <stddef.h>
#include <wchar.h>
typedef struct { int width, height; } Resolution;
#define LOCATOR_MAX_LENGTH 12
#define RESOLUTION_COUNT 10
#define DEFAULT_RESOLUTION_INDEX 2 /* 320x240 (4:3), 320x180 (16:9). */
extern const Resolution resolutions43[RESOLUTION_COUNT], resolutions169[RESOLUTION_COUNT];
extern const wchar_t *const bands[12];
extern const wchar_t *const band_files[12];
typedef struct { unsigned char red, green, blue; } ContestColor;
typedef struct { ContestColor background, foreground; } ContestPalette;
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
#endif
