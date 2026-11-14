/*
 * driver2 — full kernel payload (Driver2.sys)
 *
 * Exposes the complete Betest IOCTL table: process/kernel R/W, AC lab IOCTLs,
 * stealth hooks, DKOM (opt-in), etc. Usermode: sdk/driver2_api.h + Driver2Client.exe
 *
 * Build only from driver2\build.ps1.
 */
#define DRIVER2_PRODUCT 1
#include "../../src/driver/BetestDriver.c"
