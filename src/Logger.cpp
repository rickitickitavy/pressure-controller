/*
 * Logger.cpp
 *
 *  Created on: 26.06.2017
 *      Author: jane
 */

#include "Logger.h"
#include <stdio.h>
#include <Arduino.h>
#include <FS.h>
#include <SD.h>

#define NEW_LINE_PART_LEN 4

// Logger *LOGGER = new Logger();

Logger::Logger() {
#ifdef CON_DEBUG
    Serial.begin(115200);
    delay(300);
    Serial.println("Starting logger...");
#endif
    datetime_buffer = (char*)malloc(256);
    mini_datetime_buffer = (char*)malloc(256);
}
//------------------------------------------------------------------------------

void Logger::println(String msg) {
#ifdef CON_DEBUG
    Serial.println(msg);
#endif
}
//------------------------------------------------------------------------------

void Logger::print(String msg) {
#ifdef CON_DEBUG
    Serial.print(msg);
#endif
}
//------------------------------------------------------------------------------

void Logger::getTime() {
    tm localtm;
    if (getLocalTime(&localtm, 0)) {
        sprintf(mini_datetime_buffer, "%04d-%02d-%02d %02d:%02d:%02d "
                , localtm.tm_year + 1900, localtm.tm_mon, localtm.tm_mday
                , localtm.tm_hour, localtm.tm_min, localtm.tm_sec);
        sprintf(datetime_buffer, "<span style=\"color: gray\">%s</span> ", mini_datetime_buffer);
    } else {
        datetime_buffer[0] = 0;
        mini_datetime_buffer[0] = 0;
    }

}

void Logger::error(String msg) {
    if (logLevel <= LOG_LEVEL_ERROR) {
        getTime();
        println(String(mini_datetime_buffer) + "ERROR: " + msg);
    }
}
//------------------------------------------------------------------------------

void Logger::warning(String msg) {
    if (logLevel <= LOG_LEVEL_WARNING) {
        getTime();
        println(String(mini_datetime_buffer) + "WARNING: " + msg);
    }
}
//------------------------------------------------------------------------------

void Logger::debug(String msg) {
    if (logLevel <= LOG_LEVEL_DEBUG) {
        getTime();
        println(String(mini_datetime_buffer) + "DEBUG: " + msg);
    }
}
//------------------------------------------------------------------------------

void Logger::detailDebug(String msg) {
    if (logLevel <= LOG_LEVEL_DETAIL_DEBUG) {
        getTime();
        println(String(mini_datetime_buffer) + "DEBUG: " + msg);
    }
}
//------------------------------------------------------------------------------

void Logger::info(String msg) {
    if (logLevel <= LOG_LEVEL_INFO) {
        getTime();
        println(String(mini_datetime_buffer) + "INFO: " + msg);
    }
}
//------------------------------------------------------------------------------




