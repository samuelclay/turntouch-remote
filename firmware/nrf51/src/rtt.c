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
    sprintf(colorStringFormat, " %s--->%s %s", RTT_CTRL_TEXT_MAGENTA, RTT_CTRL_RESET, sFormat);
    va_list ParamList;

    va_start(ParamList, sFormat);
    SEGGER_RTT_vprintf(BufferIndex, colorStringFormat, &ParamList);
}