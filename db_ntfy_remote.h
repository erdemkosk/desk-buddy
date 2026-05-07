#pragma once

#include <Arduino.h>

/** Bit mask: bir = ilgili home slot karti uzaktan saklanmis (sol-ust=s0 ... sag-alt=s3). */
extern uint8_t deskRemoteHideSlotBits;

/** setup()'ta bir kez: ntfy HTTP arka plan gorevine baslar (Core 0). */
void deskNtfyInit();

/** loop() basinda/agresif: Worker kuyruk komutlari uygulanir — TFT burada güncellenir. */
void deskNtfyApplyQueued();
