//
// Created by dsporykhin on 30.04.20.
//

#include "ParamDescriptor.h"

ParamDescriptor::ParamDescriptor(String paramName, ParamType paramType, float minValue, float maxValue,
                                 void *valueReferenceForRead, void *valueReferenceForWrite) {
    this->paramName = paramName;
    this->paramType = paramType;
    this->minValue = minValue;
    this->maxValue = maxValue;
    this->valueReferenceForRead = valueReferenceForRead;
    this->valueReferenceForWrite = valueReferenceForWrite;
    this->arraySize = 0;
}

ParamDescriptor::ParamDescriptor(String paramName, ParamType paramType, float minValue, float maxValue, int arraySize,
                                 void *valueReferenceForRead, void *valueReferenceForWrite) {
    this->paramName = paramName;
    this->paramType = paramType;
    this->minValue = minValue;
    this->maxValue = maxValue;
    this->valueReferenceForRead = valueReferenceForRead;
    this->valueReferenceForWrite = valueReferenceForWrite;
    this->arraySize = arraySize;
}
