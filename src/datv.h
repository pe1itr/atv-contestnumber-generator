#ifndef ATV_DATV_H
#define ATV_DATV_H
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#define TELETEXT_ROWS 23
#define TELETEXT_COLUMNS 40
#define TELETEXT_CELLS (TELETEXT_ROWS*TELETEXT_COLUMNS)
#define TELETEXT_INPUT_SIZE (TELETEXT_CELLS+TELETEXT_ROWS*2+1)
typedef struct { int enabled; char text[TELETEXT_CELLS+1]; } TeletextSettings;
/* Flat, space-padded rows in settings; LF/CRLF in the editor. */
const char *teletext_validate(const TeletextSettings *s);
const char *teletext_from_text(TeletextSettings *s, const char *text);
void teletext_to_text(const TeletextSettings *s, char out[TELETEXT_INPUT_SIZE], int crlf);

#define EIT_CITY_LENGTH 40
#define EIT_DESCRIPTION_LENGTH 240
#define EIT_OPERATOR_LENGTH 40
typedef struct {
    char city[EIT_CITY_LENGTH*4+1], description[EIT_DESCRIPTION_LENGTH*4+1];
    char operator_name[EIT_OPERATOR_LENGTH*4+1]; /* UTF-8 */
} StationInfo;
typedef struct {
    int bitrate, seconds, fps, gop;
    int eit_enabled;
    TeletextSettings teletext;
    /* Snapshot of the station at Start/export, owned by the worker. */
    char locator[49];
    StationInfo station;
} DatvSettings;
const char *station_validate(const StationInfo *station);
typedef struct { int qp, frames, largest_idr; } DatvResult;
typedef struct { char ip[16]; int port; DatvSettings video; } DatvUdpSettings;
typedef struct DatvStream DatvStream;
enum { DATV_PREPARING, DATV_RUNNING, DATV_STOPPED, DATV_FAILED };
typedef struct {
    int state, qp;
    uint64_t packets;
    /* Reported port refusals, not a count of all datagrams lost at the receiver. */
    uint64_t refusals;
    double last_refusal_seconds;
    double seconds;
    char error[256];
} DatvUdpStatus;
/* The bitrate is the complete 188-byte TS bitrate, in bits per second. */
DatvSettings datv_defaults(void);
DatvUdpSettings datv_udp_defaults(void);
const char *datv_udp_validate(DatvUdpSettings s, int width, int height);
void datv_udp_status_text(DatvUdpSettings s, DatvUdpStatus status, char *text, size_t size);
/* Copies the card and prepares/sends on an owned worker. No GUI callbacks.
 * stop is asynchronous; destroy stops and joins. status is thread-safe.
 * Caller must serialize start/status/stop/destroy on its UI thread. */
DatvStream *datv_udp_start(const uint32_t *rgb, int width, int height, int stride,
                          const char *call, DatvUdpSettings settings, char error[256]);
/* Local preview only: same quality selection as continuous UDP, no network.
 * Poll with datv_udp_status; STOPPED means the first IDR image is ready.
 * Reuse stop/destroy for cancellation/cleanup. Decoder errors report FAILED. */
DatvStream *datv_preview_start(const uint32_t *rgb, int width, int height, int stride,
                              const char *call, DatvSettings settings, char error[256]);
/* Copy the decoded image after completion. Returns 0 until available or on bad dimensions. */
int datv_preview_image(DatvStream *job, uint32_t *rgb, int width, int height, int stride);
void datv_udp_status(DatvStream *stream, DatvUdpStatus *status);
void datv_udp_stop(DatvStream *stream);
void datv_udp_destroy(DatvStream *stream);
const char *datv_validate(DatvSettings s, int width, int height);
int datv_test_options(int count, const char *const *values, DatvSettings *s, int *w, int *h);
/* RGB words 0x00RRGGBB, top-down, stride in bytes. Output has one second
 * decoder lead-in, then `seconds` of video; rounded to seven TS packets.
 * No output is written until encoding and transport scheduling succeed.
 * error must have room for 256 bytes. No FFmpeg or external processes. */
int datv_write(FILE *output, const uint32_t *rgb, int width, int height,
               int stride, const char *call, DatvSettings settings,
               DatvResult *result, char error[256]);
#ifdef __cplusplus
}
#endif
#endif
