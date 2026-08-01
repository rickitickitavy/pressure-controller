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

#define LOGGER_SIZE 100

class Logger {
private:
    void print(String msg);

    void println(String msg);

public:
    char logData[LOGGER_SIZE + 1];
    int logLen = 0;

    char logLevel = LOG_LEVEL;

    Logger();

    void error(String msg);

    void warning(String msg);

    void info(String msg);

    void debug(String msg);

    void detailDebug(String msg);

private:
    String spanStart = "<span style=\"color: ";
    String spanEnd = "</span>";

};

extern Logger LOGGER;

#endif /* LOGGER_H_ */
