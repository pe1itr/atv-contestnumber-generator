#ifndef ATV_PM5544_H
#define ATV_PM5544_H
#include <stdint.h>
#include <wchar.h>
/* RGB words (0x00RRGGBB), suitable for Cairo RGB24 and Windows 32-bit DIBs. */
int pm5544_render(uint32_t *pixels, int width, int height,
                  const wchar_t *call, const wchar_t *locator);
/* FUBK uses its central black panel and the first six locator characters. */
int fubk_render(uint32_t *pixels, int width, int height,
                const wchar_t *call, const wchar_t *locator);
int pm5644_render(uint32_t *pixels, int width, int height,
                  const wchar_t *call, const wchar_t *locator);
void pm5544_cleanup(void);
#endif
