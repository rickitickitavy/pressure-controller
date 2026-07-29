//
// Created by dsporykhin on 24.04.20.
//

#include <IPAddress.h>
#include "SettingsNavigator.h"
#include "Logger.h"

SettingsNavigator::SettingsNavigator(SettingsManager *settingsManager) {
    this->settingsManager = settingsManager;
    this->settings = settingsManager->getSettings();

//    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("flame>shutdownAfterSec", INTEGER, 5, 78600,
//                                                                           (void *) &settings->activeProfile.flameSettings.shutdownAfterSec,
//                                                                           (void *) &tempProfile.flameSettings.shutdownAfterSec);
//    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("flame>brightnessCoefficientPrct", FLOAT, 0,
//                                                                           100,
//                                                                           (void *) &settings->activeProfile.flameSettings.brightnessCoefficientPrct,
//                                                                           (void *) &tempProfile.flameSettings.brightnessCoefficientPrct);

//    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("network>wifiMode", UCHAR, 0,
//                                                                           10,
//                                                                           (void *) &settings->network.wifiMode,
//                                                                           (void *) &settings->network.wifiMode);
    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("network>ssid", STRING, 5,
                                                                           63,
                                                                           (void *) &settings->network.ssid[0],
                                                                           (void *) &settings->network.ssid[0]);
    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("network>password", STRING, 5,
                                                                           63,
                                                                           (void *) &settings->network.password[0],
                                                                           (void *) &settings->network.password[0]);
    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("network>hostName", STRING, 5,
                                                                           63,
                                                                           (void *) &settings->network.hostName[0],
                                                                           (void *) &settings->network.hostName[0]);

    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("mqtt>server", STRING, 5,
                                                                           63,
                                                                           (void *) &settings->mqttServer[0],
                                                                           (void *) &settings->mqttServer[0]);
    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("mqtt>port", INTEGER, 30, 65534,
                                                                           (void *) &settings->mqttPort,
                                                                           (void *) &settings->mqttPort);
    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("mqtt>reconnectIntervalMs", INTEGER, 30, 60000,
                                                                           (void *) &settings->mqttReconnectIntervalMs,
                                                                           (void *) &settings->mqttReconnectIntervalMs);
    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("mqtt>deviceName", STRING, 2,
                                                                           31,
                                                                           (void *) &settings->mqttDeviceName[0],
                                                                           (void *) &settings->mqttDeviceName[0]);
    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("mqtt>topicTheDeviceIsAlive", STRING, 2,
                                                                           31,
                                                                           (void *) &settings->topicTheDeviceIsAlive[0],
                                                                           (void *) &settings->topicTheDeviceIsAlive[0]);
    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("mqtt>topicTheDeviceState", STRING, 2,
                                                                           31,
                                                                           (void *) &settings->topicTheDeviceState[0],
                                                                           (void *) &settings->topicTheDeviceState[0]);
    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("mqtt>topicToListenCommands", STRING, 2,
                                                                           31,
                                                                           (void *) &settings->topicToListenCommands[0],
                                                                           (void *) &settings->topicToListenCommands[0]);
    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("mqtt>topicToListenServerWasBorn", STRING, 2,
                                                                           31,
                                                                           (void *) &settings->topicToListenServerWasBorn[0],
                                                                           (void *) &settings->topicToListenServerWasBorn[0]);
    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("device>bright", FLOAT, 0.01f, 100.f,
                                                                           (void *) &settings->bright,
                                                                           (void *) &settings->bright);
//    this->paramDescriptors[activeParamDescriptors++] = new ParamDescriptor("network>ipAddress", IPv4, 7,
//                                                                           15,
//                                                                           (void *) &settings->network.ipAddress[0],
//                                                                           (void *) &settings->network.ipAddress[0]);

}
//--------------------------------------------------------------------

String SettingsNavigator::getSettingByName(String origParamName) {
    String paramName = origParamName;

    bool showMinR = paramName.endsWith("#minR");
    bool showMin = paramName.endsWith("#min");
    bool showMaxR = paramName.endsWith("#maxR");
    bool showMax = paramName.endsWith("#max");

    // проверим на наличие #min или #max в конце имени переменной
    if (showMax || showMin) {
        paramName.remove(origParamName.length() - 4);
    }  else if (showMaxR || showMinR) {
        paramName.remove(origParamName.length() - 5);
    }

    for (int descriptorIndex = 0; descriptorIndex < activeParamDescriptors; descriptorIndex++) {
        if (paramDescriptors[descriptorIndex]->paramName == paramName) {
            // Нашли наш параметер
            if (paramDescriptors[descriptorIndex]->arraySize == 0 || showMax || showMin || showMaxR || showMinR) {
                if (paramDescriptors[descriptorIndex]->paramType == INTEGER || showMaxR || showMinR) {
                    return showMin || showMax || showMinR || showMaxR
                           ? (showMin || showMinR ? String((int) paramDescriptors[descriptorIndex]->minValue) : String(
                                    (int) paramDescriptors[descriptorIndex]->maxValue))
                           : String(*(int *) paramDescriptors[descriptorIndex]->valueReferenceForRead);
                } else if (paramDescriptors[descriptorIndex]->paramType == FLOAT) {
                    return showMin || showMax
                           ? (showMin ? String(paramDescriptors[descriptorIndex]->minValue) : String(
                                    paramDescriptors[descriptorIndex]->maxValue))
                           : String(*(float *) paramDescriptors[descriptorIndex]->valueReferenceForRead);
                } else if (paramDescriptors[descriptorIndex]->paramType == STRING){
                    return showMin || showMax
                           ? (showMin ? String((int)paramDescriptors[descriptorIndex]->minValue) : String(
                                    (int)paramDescriptors[descriptorIndex]->maxValue))
                           : String((char *)paramDescriptors[descriptorIndex]->valueReferenceForRead);
                } else if (paramDescriptors[descriptorIndex]->paramType == UCHAR){
                    return showMin || showMax
                           ? (showMin ? String((unsigned char)paramDescriptors[descriptorIndex]->minValue) : String(
                                    (unsigned char)paramDescriptors[descriptorIndex]->maxValue))
                           : String(*(unsigned char *)paramDescriptors[descriptorIndex]->valueReferenceForRead);
                } else if (paramDescriptors[descriptorIndex]->paramType == IPv4){
                    String ipAsString;

                    if (!showMin && !showMax){
                        unsigned char *ipRaw = (unsigned char *)paramDescriptors[descriptorIndex]->valueReferenceForRead;
                        IPAddress ipV4 = IPAddress(ipRaw[0], ipRaw[1], ipRaw[2], ipRaw[3]);
                        ipAsString = ipV4.toString();
                    }
                    return showMin || showMax
                           ? (showMin ? String((unsigned char)paramDescriptors[descriptorIndex]->minValue) : String(
                                    (unsigned char)paramDescriptors[descriptorIndex]->maxValue))
                           : ipAsString;
                }
            } else {
                //  array
                String paramNameTitle = paramName;
                paramNameTitle.replace('>', '_');
                String result = "{\"" + paramNameTitle + "\":[";

                if (paramDescriptors[descriptorIndex]->paramType == INTEGER) {
                    int *arrayRef = (int *) paramDescriptors[descriptorIndex]->valueReferenceForRead;

                    for (int i = 0; i < paramDescriptors[descriptorIndex]->arraySize; i++) {
                        if (i != 0) {
                            result.concat(",");
                        }
                        result.concat(String(arrayRef[i]));
                    }
                } else if (paramDescriptors[descriptorIndex]->paramType == FLOAT) {
                    float *arrayRef = (float *) paramDescriptors[descriptorIndex]->valueReferenceForRead;

                    for (int i = 0; i < paramDescriptors[descriptorIndex]->arraySize; i++) {
                        if (i != 0) {
                            result.concat(",");
                        }
                        result.concat(String(arrayRef[i]));
                    }
                }
                result.concat("]}");
                return result;
            }
        }
    }

    LOGGER.error("parameter " + paramName + " not found");
    return "bad parameter name";

}
//--------------------------------------------------------------------


void SettingsNavigator::saveNetworkSettingsAndRestart(NetworkSettings *networkSettings) {
    LOGGER.warning("Saving new network settings:\r\n"
                           "        SSID: " + String(networkSettings->ssid) + "\r\n"
                           "    password: " + String(networkSettings->password) + "\r\n"
                           "   host name: " + String(networkSettings->hostName));

    memcpy(&settings->network, networkSettings, sizeof(NetworkSettings));

    settingsManager->saveSetting(true);
}
//--------------------------------------------------------------------


String SettingsNavigator::saveSettingsByNames(String *params, int paramsCount) {
    String res = "";

    for (int paramIndex = 0; paramIndex < paramsCount; paramIndex++) {
        String param = params[paramIndex];
        param.trim();

        if (!param.isEmpty()) {
// отделим значение от имени параметра
            String invalidParameterMessage = ("Invalid parameter format (\"" + param + "\"");
            int separatorIndex = param.indexOf('=');
            if (separatorIndex == -1) {
                return invalidParameterMessage;
            }

            String paramName = param.substring(0, separatorIndex);
            String value = param.substring(separatorIndex + 1);

            paramName.trim();
            value.trim();

            if (param.isEmpty() || value.isEmpty()) {
                return invalidParameterMessage;
            }

            char tempBuf[64];
            res = saveSettingByName(paramName, value);
        }
        // Если ошибка записи переменной, то далее не продолжаем
        if (res != "") {
            return res;
        }
    }

    settingsManager->saveSetting(false);

    settingsManager->logSettings();

    return res;
}
//--------------------------------------------------------------------

String SettingsNavigator::saveSettingByName(String paramName, String value) {
    for (int descriptorIndex = 0; descriptorIndex < activeParamDescriptors; descriptorIndex++) {
        if (paramDescriptors[descriptorIndex]->paramName == paramName) {
            // Нашли наш параметер
            if (paramDescriptors[descriptorIndex]->arraySize == 0) {
                if (paramDescriptors[descriptorIndex]->paramType == INTEGER) {
                    int intValue = value.toInt();
                    if ((intValue < paramDescriptors[descriptorIndex]->minValue)
                        || (intValue > paramDescriptors[descriptorIndex]->maxValue)) {
                        return "Value of \"" + paramName + "\" is not in diapason from "
                               + String(paramDescriptors[descriptorIndex]->minValue) + " to "
                               + String(paramDescriptors[descriptorIndex]->maxValue);
                    } else {
                        *((int *) paramDescriptors[descriptorIndex]->valueReferenceForWrite) = intValue;
                    }
                } else if (paramDescriptors[descriptorIndex]->paramType == FLOAT) {
                    float floatValue = value.toFloat();
                    if ((floatValue < paramDescriptors[descriptorIndex]->minValue)
                        || (floatValue > paramDescriptors[descriptorIndex]->maxValue)) {
                        return "Value of \"" + paramName + "\" is not in diapason from "
                               + String(paramDescriptors[descriptorIndex]->minValue) + " to "
                               + String(paramDescriptors[descriptorIndex]->maxValue);
                    } else {
                        *((float *) paramDescriptors[descriptorIndex]->valueReferenceForWrite) = floatValue;
                    }
                } else if (paramDescriptors[descriptorIndex]->paramType == STRING){
                    int valLen = value.length();
                    if ((valLen < paramDescriptors[descriptorIndex]->minValue)
                            || (valLen > paramDescriptors[descriptorIndex]->maxValue)){
                        return "Length of \"" + paramName + "\" is not in diapason from "
                               + String(paramDescriptors[descriptorIndex]->minValue) + " to "
                               + String(paramDescriptors[descriptorIndex]->maxValue);
                    } else {
                        memset(paramDescriptors[descriptorIndex]->valueReferenceForWrite, 0, valLen+1);
                        memcpy(paramDescriptors[descriptorIndex]->valueReferenceForWrite, value.c_str(), valLen);
                    }
                }
            } else {
                // массив
                if (paramDescriptors[descriptorIndex]->paramType == FLOAT) {

                    char elementSeparator = ',';

                    int indexOfParamSeparator = value.indexOf(elementSeparator);
                    int startCopyIndex = 0;
                    int matrixIndex = 0;
                    String strValue;
                    if (value.indexOf(elementSeparator) >= 0)
                        do {
                            strValue = indexOfParamSeparator == -1
                                       ? value.substring(startCopyIndex)
                                       : value.substring(startCopyIndex, indexOfParamSeparator);
                            startCopyIndex = indexOfParamSeparator + 1;

                            float *refMatrix = (float *) paramDescriptors[descriptorIndex]->valueReferenceForWrite;
                            float floatValue = strValue.toFloat();

                            if ((floatValue < paramDescriptors[descriptorIndex]->minValue)
                                || (floatValue > paramDescriptors[descriptorIndex]->maxValue)) {
                                return "Value of \"" + paramName + "\" is not in diapason from "
                                       + String(paramDescriptors[descriptorIndex]->minValue) + " to "
                                       + String(paramDescriptors[descriptorIndex]->maxValue);
                            }
                            refMatrix[matrixIndex] = floatValue;
                            matrixIndex++;
                        } while ((indexOfParamSeparator = value.indexOf(elementSeparator, indexOfParamSeparator + 1)) >=
                                 0 &&
                                 matrixIndex < 32);
                    // последний элемент
                    if (startCopyIndex < value.length() && matrixIndex < 32) {
                        strValue = indexOfParamSeparator == -1
                                   ? value.substring(startCopyIndex)
                                   : value.substring(startCopyIndex, indexOfParamSeparator);

                        float *refMatrix = (float *) paramDescriptors[descriptorIndex]->valueReferenceForWrite;
                        float floatValue = strValue.toFloat();

                        if ((floatValue < paramDescriptors[descriptorIndex]->minValue)
                            || (floatValue > paramDescriptors[descriptorIndex]->maxValue)) {
                            return "Value of \"" + paramName + "\" is not in diapason from "
                                   + String(paramDescriptors[descriptorIndex]->minValue) + " to "
                                   + String(paramDescriptors[descriptorIndex]->maxValue);
                        }
                        refMatrix[matrixIndex] = floatValue;
                    }
                }
            }

            return "";
        }
    }
    return "parameter \"" + paramName + "\" not found";
}
//--------------------------------------------------------------------

