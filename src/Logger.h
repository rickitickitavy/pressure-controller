/*
 * Logger.h
 *
 *  Created on: 26.06.2017
 *      Author: jane
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <Arduino.h>
#include "Defines.h"
#include <FS.h>


#define CON_DEBUG_LEVEL_ALL 1
#define LOG_LEVEL_DETAIL_DEBUG 0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_WARNING 3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_NOTHING 5

#define CON_DEBUG true


class Logger {
private:
    File log_file;

    void print(String msg);

    void println(String msg);

    void getTime();

    char *datetime_buffer;
    char *mini_datetime_buffer;

public:
    char logLevel = LOG_LEVEL;

    Logger();

    void error(String msg);

    void warning(String msg);

    void info(String msg);

    void debug(String msg);

    void detailDebug(String msg);

};

// extern Logger LOGGER;

#endif /* LOGGER_H_ */
