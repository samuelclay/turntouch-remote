/* Copyright 2015 Turn Touch. All rights reserved.
 * 
 * Source for RTT debugging.
 * 
 */

#include <stdarg.h>
#include <stdio.h>
#include "rtt.h"

void rtt_print(unsigned BufferIndex, const char * sFormat, ...) {
    char colorStringFormat[256];
    sprintf(colorStringFormat, "%s %s--->%s %s%s", RTT_CTRL_RESET, RTT_CTRL_TEXT_MAGENTA, RTT_CTRL_RESET, sFormat, RTT_CTRL_RESET);
    va_list ParamList;

    va_start(ParamList, sFormat);
    SEGGER_RTT_vprintf(BufferIndex, colorStringFormat, &ParamList);
}

void rtt_log(const char * str, ...) {
    char colorStringFormat[256];
    sprintf(colorStringFormat, "%s %s--->%s %s%s", RTT_CTRL_RESET, RTT_CTRL_TEXT_MAGENTA, RTT_CTRL_RESET, str, RTT_CTRL_RESET);
    va_list ParamList;

    va_start(ParamList, str);
    SEGGER_RTT_vprintf(0, colorStringFormat, &ParamList);
}
