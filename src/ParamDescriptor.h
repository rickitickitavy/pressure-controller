//
// Created by dsporykhin on 30.04.20.
//

#ifndef EFLAMEESP8266_PARAMDESCRIPTOR_H
#define EFLAMEESP8266_PARAMDESCRIPTOR_H

#include "SettingsNavigator.h"
#include "Defines.h"


enum ParamType{
    INTEGER, FLOAT, BOOLEAN, STRING, UCHAR, IPv4
}
;

class ParamDescriptor {
public:
    String paramName;
    ParamType paramType;
    float minValue;
    float maxValue;
    int arraySize;
    void *valueReferenceForRead;
    void *valueReferenceForWrite;

//    {new ParamDescriptor("flame>loopIntervalMs", INTEGER, 2, 300
//                , (void *) &settings->activeProfile.flameSettings.loopIntervalMs, (void *) &tempProfile.flameSettings.loopIntervalMs)}

    ParamDescriptor(String paramName, ParamType paramType, float minValue, float maxValue,
                    void *valueReferenceForRead, void *valueReferenceForWrite);

    ParamDescriptor(String paramName, ParamType paramType, float minValue, float maxValue, int arraySize,
                    void *valueReferenceForRead, void *valueReferenceForWrite);
};

#endif //EFLAMEESP8266_PARAMDESCRIPTOR_H
