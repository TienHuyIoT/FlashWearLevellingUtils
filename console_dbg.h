#ifndef __CONSOLE_DEBUG_H
#define __CONSOLE_DEBUG_H

#if defined(ARDUINO)
#define DBG_PRINTF(f_, ...)           Serial.printf((f_), ##__VA_ARGS__)
#elif defined(__MBED__)
#include "mbed.h"
// https://os.mbed.com/users/GlimwormBeacons/code/SEGGER_RTT/
#include "Segger_rtt/SEGGER_RTT.h"
#define DBG_PRINTF(f_, ...)           SEGGER_RTT_printf(0, (f_), ##__VA_ARGS__)
#endif

#define g_debugLevel 4

#define CONSOLE_LOGE(...) {if(g_debugLevel >= 0) {DBG_PRINTF(__VA_ARGS__);}}
#define CONSOLE_LOGW(...) {if(g_debugLevel >= 1) {DBG_PRINTF(__VA_ARGS__);}}
#define CONSOLE_LOGI(...) {if(g_debugLevel >= 2) {DBG_PRINTF(__VA_ARGS__);}}
#define CONSOLE_LOGD(...) {if(g_debugLevel >= 3) {DBG_PRINTF(__VA_ARGS__);}}
#define CONSOLE_LOGV(...) {if(g_debugLevel >= 4) {DBG_PRINTF(__VA_ARGS__);}}

#define CONSOLE_TAG_LOGE(x, ...) {if(g_debugLevel >= 0) {DBG_PRINTF("E %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}
#define CONSOLE_TAG_LOGW(x, ...) {if(g_debugLevel >= 1) {DBG_PRINTF("W %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}
#define CONSOLE_TAG_LOGI(x, ...) {if(g_debugLevel >= 2) {DBG_PRINTF("I %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}
#define CONSOLE_TAG_LOGD(x, ...) {if(g_debugLevel >= 3) {DBG_PRINTF("D %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}
#define CONSOLE_TAG_LOGV(x, ...) {if(g_debugLevel >= 4) {DBG_PRINTF("V %s: ",x); DBG_PRINTF(__VA_ARGS__); DBG_PRINTF("\r\n");}}

#endif // __CONSOLE_DEBUG_H
