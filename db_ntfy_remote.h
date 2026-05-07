#pragma once

#include <Arduino.h>

/** Bit mask: bir = ilgili home slot karti uzaktan saklanmis (sol-ust=s0 ... sag-alt=s3). */
extern uint8_t deskRemoteHideSlotBits;

/** loop() icinde cagirin; WIFI bagliyken POLL + komutlari isler. Kesin bloklamaz HTTP timeout sinirlar. */
void deskNtfyPollIfDue();
