/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (OFFWORLD) */
/* STRANGER SYNTH by swtdr34mz (based on OFFWORLD): a performance synth for the M-VAVE FM-1 (on fm1-base). Colossal 80s brass (two detuned saws, a
 * filter that swells like a horn section, ensemble chorus), a reverb up to 30 s with FREEZE, and rain.
 * This header is the app's half of the contract with the platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "STRANGER SYNTH"
#define APP_SPLASH_RGB RGB(255, 40, 40)
#define APP_SPLASH                       /* app_splash: the neon title, not the platform's page */
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz (a drone: no hurry) */
#define APP_OVERLOAD                     /* app_overload: the render over 90 % of its time: a voice let go */
#define APP_TOUCH                        /* velocity from the keys (lib/touch.h): harder = brighter brass */

/* the store: the app's settings and the panel calibration (the platform's) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

/* backup and restore over USB: the settings */
#define APP_BACKUP_OBJS {OBJ_SETTINGS}
#define APP_BACKUP_MAGIC 0x4E525453u            /* "STRN" */
#define APP_BACKUP_VERSION 1u
#define APP_BACKUP_WHAT "replaces presets and settings"
int app_backup_playing(void);
void app_backup_flush(void);
void app_backup_hold(int on);
void app_backup_restored(void);

void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
void app_overload(void);
void app_splash(void);

#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
