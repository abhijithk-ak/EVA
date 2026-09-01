#pragma once
/*
 * Logger.h
 * ---------------------------------------------------------
 * Minimal, zero-cost-when-disabled logging. Set EVA_DEBUG to
 * 0 in Config.h for production builds to remove all logging
 * overhead.
 * ---------------------------------------------------------
 */

#include "Config.h"

#if EVA_DEBUG
    #define EVA_LOG_BEGIN()      Serial.begin(EVA_SERIAL_BAUD)
    #define EVA_LOGLN(msg)       Serial.println(msg)
    #define EVA_LOG(msg)         Serial.print(msg)
    #define EVA_LOGF(fmt, ...)   Serial.printf(fmt, ##__VA_ARGS__)
#else
    #define EVA_LOG_BEGIN()
    #define EVA_LOGLN(msg)
    #define EVA_LOG(msg)
    #define EVA_LOGF(fmt, ...)
#endif
