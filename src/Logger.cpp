/*
 * Logger.cpp
 *
 *  Created on: 26.06.2017
 *      Author: jane
 */

#include "Logger.h"
#include <Arduino.h>

#define NEW_LINE_PART_LEN 4

Logger LOGGER;

Logger::Logger() {
#ifdef CON_DEBUG
    Serial.begin(115200);
    Serial.println("---");
#endif
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

void Logger::error(String msg) {
    if (logLevel <= LOG_LEVEL_ERROR) {
        println("ERROR: " + msg);
    }
}
//------------------------------------------------------------------------------

void Logger::warning(String msg) {
    if (logLevel <= LOG_LEVEL_WARNING) {
        println("WARNING: " + msg);
    }
}
//------------------------------------------------------------------------------

void Logger::debug(String msg) {
    if (logLevel <= LOG_LEVEL_DEBUG) {
        println("DEBUG: " + msg);
    }
}
//------------------------------------------------------------------------------

void Logger::detailDebug(String msg) {
    if (logLevel <= LOG_LEVEL_DETAIL_DEBUG) {
        println("DEBUG: " + msg);
    }
}
//------------------------------------------------------------------------------

void Logger::info(String msg) {
    if (logLevel <= LOG_LEVEL_INFO) {
        println("INFO: " + msg);
    }
}
//------------------------------------------------------------------------------




