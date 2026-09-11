#ifndef ATV_CORE_H
#define ATV_CORE_H
#include <stddef.h>
#include <wchar.h>
typedef struct { int width, height; } Resolution;
#define RESOLUTION_COUNT 10
#define DEFAULT_RESOLUTION_INDEX 2 /* 320x240 (4:3), 320x180 (16:9). */
extern const Resolution resolutions43[RESOLUTION_COUNT], resolutions169[RESOLUTION_COUNT];
extern const wchar_t *const bands[12];
extern const wchar_t *const band_files[12];
int valid_call(const wchar_t *s);
int valid_locator(const wchar_t *s);
int valid_code(const wchar_t *s);
int code_digit_sum(const wchar_t *s);
int generated_code_valid(unsigned code);
void filename_call(wchar_t *out, const wchar_t *in);
#endif
