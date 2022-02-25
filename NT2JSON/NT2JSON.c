/*
 * Copyright 2022 Napatech A/S. All rights reserved.
 * 
 * 1. Copying, modification, and distribution of this file, or executable
 * versions of this file, is governed by the terms of the Napatech Software
 * license agreement under which this file was made available. If you do not
 * agree to the terms of the license do not install, copy, access or
 * otherwise use this file.
 * 
 * 2. Under the Napatech Software license agreement you are granted a
 * limited, non-exclusive, non-assignable, copyright license to copy, modify
 * and distribute this file in conjunction with Napatech SmartNIC's and
 * similar hardware manufactured or supplied by Napatech A/S.
 * 
 * 3. The full Napatech Software license agreement is included in this
 * distribution, please see "NP-0405 Napatech Software license
 * agreement.pdf"
 * 
 * 4. Redistributions of source code must retain this copyright notice,
 * list of conditions and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTIES, EXPRESS OR
 * IMPLIED, AND NAPATECH DISCLAIMS ALL IMPLIED WARRANTIES INCLUDING ANY
 * IMPLIED WARRANTY OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, OR OF
 * FITNESS FOR A PARTICULAR PURPOSE. TO THE EXTENT NOT PROHIBITED BY
 * APPLICABLE LAW, IN NO EVENT SHALL NAPATECH BE LIABLE FOR PERSONAL INJURY,
 * OR ANY INCIDENTAL, SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES WHATSOEVER,
 * INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF PROFITS, CORRUPTION OR
 * LOSS OF DATA, FAILURE TO TRANSMIT OR RECEIVE ANY DATA OR INFORMATION,
 * BUSINESS INTERRUPTION OR ANY OTHER COMMERCIAL DAMAGES OR LOSSES, ARISING
 * OUT OF OR RELATED TO YOUR USE OR INABILITY TO USE NAPATECH SOFTWARE OR
 * SERVICES OR ANY THIRD PARTY SOFTWARE OR APPLICATIONS IN CONJUNCTION WITH
 * THE NAPATECH SOFTWARE OR SERVICES, HOWEVER CAUSED, REGARDLESS OF THE THEORY
 * OF LIABILITY (CONTRACT, TORT OR OTHERWISE) AND EVEN IF NAPATECH HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGES. SOME JURISDICTIONS DO NOT ALLOW
 * THE EXCLUSION OR LIMITATION OF LIABILITY FOR PERSONAL INJURY, OR OF
 * INCIDENTAL OR CONSEQUENTIAL DAMAGES, SO THIS LIMITATION MAY NOT APPLY TO YOU.
 */




#ifdef WIN32
#include <WinSock2.h>
#include "os.h"
#else
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <signal.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h> // MIN
#include <sys/time.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

typedef int SOCKET;

#endif

#include <getopt.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <json-c/json.h>

#include <nt.h>
#include "NT2JSON.h"


#define NUM_COLOR_COUNTERS 64
#define FILENAME_MAXLEN 256



/******************************************************************************/
/* Internal variables                                                         */
/******************************************************************************/
static uint appRunning = 1;

#define OPTION_HELP             (1<<1)

#define OPTION_SHORT_HELP       'h'

#define OPTION_FILE             'f'
static uint opt_filename = false;

#define OPTION_PRETTY           'p'
static uint opt_pretty_print = false;

#define OPTION_VERBOSE          'v'
static uint opt_verbose = false;

#define OPTION_SYSTEM   (1<<2)
static uint opt_system = false;

#define OPTION_TIMESYNC   (1<<3)
static uint opt_timesync = false;

#define OPTION_SENSORS   (1<<4)
static uint opt_sensors = false;

#define OPTION_ALARM   (1<<5)
static uint opt_alarm = false;

#define OPTION_PORT   (1<<6)
static uint opt_port = false;

#define OPTION_RXPORT   (1<<7)
static uint opt_rxport = false;

#define OPTION_TXPORT   (1<<8)
static uint opt_txport = false;

#define OPTION_PORTSIMPLE   (1<<9)
static uint opt_portsummary = false;

#define OPTION_STREAM   (1<<10)
static uint opt_stream = false;

#define OPTION_COLOR   (1<<11)
static uint opt_color = false;

#define OPTION_MERGEDCOLOR   (1<<12)
static uint opt_mergedcolor = false;

#define OPTION_PRODUCT (1<<13)
static uint opt_product = false;


/**
 * Table of valid options. Mainly used for the getopt_long_only
 * function. For further info see the manpage.
 */
static struct option long_options[] = {
    {"help", no_argument, 0, OPTION_HELP},
    {"system", no_argument, 0, OPTION_SYSTEM},
    {"timesync", no_argument, 0, OPTION_TIMESYNC},
    {"sensors", no_argument, 0, OPTION_SENSORS},
    {"alarm", no_argument, 0, OPTION_ALARM},
    {"port", no_argument, 0, OPTION_PORT},
    {"rxport", no_argument, 0, OPTION_RXPORT},
    {"txport", no_argument, 0, OPTION_TXPORT},
    {"portsummary", no_argument, 0, OPTION_PORTSIMPLE},
    {"stream", no_argument, 0, OPTION_STREAM},
    {"color", no_argument, 0, OPTION_COLOR},
    {"mergedcolor", no_argument, 0, OPTION_MERGEDCOLOR},
    {"product", no_argument, 0, OPTION_PRODUCT},
    {0, 0, 0, 0}
};


/*
 * The function called when user is pressing CTRL-C
 */
#ifdef WIN32
static BOOL WINAPI StopApplication(int sig)
#else

static void StopApplication(int sig)
#endif
{
    printf("\nStopping application (%d)\n", sig);
    appRunning = 0;
#ifdef WIN32
    return TRUE;
#endif
}

static void _Usage(void) {
    fprintf(stderr, "%s\n", usageText);
    exit(1);
}

// For debugging...
#if 0

static void trace(char *msg, struct json_object *jsonObj) {
    if (opt_verbose) {
        printf("--------------------------------------\n");
        printf("%s: \n%s\n", msg, json_object_to_json_string_ext(jsonObj, JSON_C_TO_STRING_PRETTY));
    }
}
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//

static struct json_object *getSystemInfo(NtInfoStream_t hInfo,
        uint32_t *numAdapters,
        uint32_t *numPorts) {
    NtInfo_t hInfoSys;

    struct json_object *jsonData;
    char buffer[NT_ERRBUF_SIZE]; // Error buffer
    int status;

    // Read the system info
    hInfoSys.cmd = NT_INFO_CMD_READ_SYSTEM;
    if ((status = NT_InfoRead(hInfo, &hInfoSys)) != NT_SUCCESS) {
        NT_ExplainError(status, buffer, sizeof (buffer) - 1);
        fprintf(stderr, "NT_InfoRead() failed: %s\n", buffer);
    }

    jsonData = json_object_new_object();

    json_object_object_add(jsonData, "numAdapters",
            json_object_new_int((int) hInfoSys.u.system.data.numAdapters));

    json_object_object_add(jsonData, "numNumaNodes",
            json_object_new_int((int) hInfoSys.u.system.data.numNumaNodes));

    json_object_object_add(jsonData, "numPorts",
            json_object_new_int((int) hInfoSys.u.system.data.numPorts));

    snprintf(buffer, NT_ERRBUF_SIZE, "%d.%d.%d.%d",
            hInfoSys.u.system.data.version.major,
            hInfoSys.u.system.data.version.minor,
            hInfoSys.u.system.data.version.patch,
            hInfoSys.u.system.data.version.tag
            );
    json_object_object_add(jsonData, "version", json_object_new_string(buffer));

    if (numAdapters != NULL) {
        *numAdapters = (uint32_t) hInfoSys.u.system.data.numAdapters;
    }

    if (numPorts != NULL) {
        *numPorts = (uint32_t) hInfoSys.u.system.data.numPorts;
    }

    return jsonData;
}

static char *decodeTimeStamp(uint64_t ts, char *ts_buf) {
    time_t tmp_ts;
    struct tm *loc_time;
    //  char ts_buf[256];

    tmp_ts = (time_t) (ts / 100000000);
    loc_time = localtime(&tmp_ts);
    snprintf(ts_buf, 32, "%04d/%02d/%02d-%02d:%02d:%02d.%09lu",
            loc_time->tm_year + 1900, loc_time->tm_mon + 1, loc_time->tm_mday,
            loc_time->tm_hour, loc_time->tm_min, loc_time->tm_sec,
            (unsigned long) (ts % 100000000 * 10));
    //printf("0x%lx: %s\n", ts, ts_buf);
    return ts_buf;
}

static char *decodeTimeStampType(uint64_t ts, char *buffer) {

    switch (ts) {
        case NT_TIMESTAMP_TYPE_NATIVE: snprintf(buffer, NT_ERRBUF_SIZE, "NATIVE");
            break;
        case NT_TIMESTAMP_TYPE_NATIVE_NDIS: snprintf(buffer, NT_ERRBUF_SIZE, "NDIS");
            break;
        case NT_TIMESTAMP_TYPE_NATIVE_UNIX: snprintf(buffer, NT_ERRBUF_SIZE, "UNIX");
            break;
        case NT_TIMESTAMP_TYPE_PCAP: snprintf(buffer, NT_ERRBUF_SIZE, "PCAP");
            break;
        case NT_TIMESTAMP_TYPE_PCAP_NANOTIME: snprintf(buffer, NT_ERRBUF_SIZE, "PCAP_NANOTIME");
            break;
        default: snprintf(buffer, NT_ERRBUF_SIZE, "Error");
    }

    //printf("ts_type: %s\n", buffer);
    return buffer;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//

static struct json_object *getProductInfo(const NtInfoStream_t hInfoStream, uint8_t adapterNo) {
    NtInfo_t hProdInfo;
    struct json_object *jsonProdInfo = json_object_new_object();

    // The file stream_info.h defines 'struct NtInfoProductInfo_s'
    //   that provides product information.
    const struct NtInfoProductInfo_v1_s *pi = &hProdInfo.u.productInfo_v1.data;
    int status;
    char imageStr[1024]; // Ensure plenty of space

    hProdInfo.cmd = NT_INFO_CMD_READ_PRODUCT_INFO_V1;
    hProdInfo.u.productInfo_v1.adapterNo = adapterNo;

    status = NT_InfoRead(hInfoStream, &hProdInfo);
    if (status != NT_SUCCESS)
        return NULL;

    if (pi->infoType == NT_PRODUCT_INFO_TYPE_GEN1) {
        json_object_object_add(jsonProdInfo, "Product No", json_object_new_string(pi->ProductId));
        json_object_object_add(jsonProdInfo, "Serial No", json_object_new_string(pi->SerialNo[0]));
        json_object_object_add(jsonProdInfo, "PBA", json_object_new_string(pi->PbaId[0]));
        json_object_object_add(jsonProdInfo, "CPLD", json_object_new_string(pi->CpldVersion));

        assert(sizeof (imageStr) >= sizeof (pi->fpgaId1));
        (void) memcpy(imageStr, pi->fpgaId1, sizeof (pi->fpgaId1));
        imageStr[14] = '\0';
        json_object_object_add(jsonProdInfo, "FPGA flash image #0", json_object_new_string(imageStr));

        assert(sizeof (imageStr) >= sizeof (pi->fpgaId2));
        (void) memcpy(imageStr, pi->fpgaId2, sizeof (pi->fpgaId2));
        imageStr[14] = '\0';
        json_object_object_add(jsonProdInfo, "FPGA flash image #1", json_object_new_string(imageStr));

    } else if (pi->infoType == NT_PRODUCT_INFO_TYPE_GEN2) {
        json_object_object_add(jsonProdInfo, "Product No", json_object_new_string(pi->ProductId));
        json_object_object_add(jsonProdInfo, "Main Board Serial No", json_object_new_string(pi->SerialNo[0]));
        json_object_object_add(jsonProdInfo, "Main Board PBA", json_object_new_string(pi->PbaId[0]));

        assert(sizeof (imageStr) >= sizeof (pi->AvrId[0]));
        (void) memcpy(imageStr, pi->AvrId[0], sizeof (pi->AvrId[0]));
        imageStr[3] = '\0';
        json_object_object_add(jsonProdInfo, "Main Board AVR", json_object_new_string(imageStr));

        assert(sizeof (imageStr) >= sizeof (pi->fpgaId1));
        (void) memcpy(imageStr, pi->fpgaId1, sizeof (pi->fpgaId1));
        imageStr[14] = '\0';
        json_object_object_add(jsonProdInfo, "FPGA flash image #0", json_object_new_string(imageStr));

        assert(sizeof (imageStr) >= sizeof (pi->fpgaId2));
        (void) memcpy(imageStr, pi->fpgaId2, sizeof (pi->fpgaId2));
        imageStr[14] = '\0';
        json_object_object_add(jsonProdInfo, "FPGA flash image #1", json_object_new_string(imageStr));
        json_object_object_add(jsonProdInfo, "Front Board Serial No", json_object_new_string(pi->SerialNo[1]));
        json_object_object_add(jsonProdInfo, "Front Board PBA", json_object_new_string(pi->PbaId[1]));

        assert(sizeof (imageStr) >= sizeof (pi->AvrId[1]));
        (void) memcpy(imageStr, pi->AvrId[1], sizeof (pi->AvrId[1]));
        imageStr[3] = '\0';
        json_object_object_add(jsonProdInfo, "Front Board AVR", json_object_new_string(imageStr));

    } else if (pi->infoType == NT_PRODUCT_INFO_TYPE_GEN3) {
        json_object_object_add(jsonProdInfo, "Product No", json_object_new_string(pi->ProductId));
        json_object_object_add(jsonProdInfo, "Serial No", json_object_new_string(pi->SerialNo[0]));
        json_object_object_add(jsonProdInfo, "PBA", json_object_new_string(pi->PbaId[0]));

        assert(sizeof (imageStr) >= sizeof (pi->AvrId[0]));
        (void) memcpy(imageStr, pi->AvrId[0], sizeof (pi->AvrId[0]));
        imageStr[3] = '\0';
        json_object_object_add(jsonProdInfo, "AVR Version", json_object_new_string(imageStr));

        assert(sizeof (imageStr) >= sizeof (pi->fpgaId1));
        (void) memcpy(imageStr, pi->fpgaId1, sizeof (pi->fpgaId1));
        imageStr[14] = '\0';
        json_object_object_add(jsonProdInfo, "FPGA flash image #0", json_object_new_string(imageStr));

        assert(sizeof (imageStr) >= sizeof (pi->fpgaId2));
        (void) memcpy(imageStr, pi->fpgaId2, sizeof (pi->fpgaId2));
        imageStr[14] = '\0';
        json_object_object_add(jsonProdInfo, "FPGA flash image #1", json_object_new_string(imageStr));

    } else {
        // Product/Info type is unknown
        assert(0);
        return NULL;
    }

    return jsonProdInfo;
}

static struct json_object * getSingleSensorData(NtInfoSensor_t *pSensor) {
    float fdivFac;
    struct json_object *jsonSensor;
    jsonSensor = json_object_new_object();

    switch (pSensor->subType) {
        case NT_SENSOR_SUBTYPE_POWER_OMA:
            json_object_object_add(jsonSensor, pSensor->name, json_object_new_string("OMA"));
            break;
        case NT_SENSOR_SUBTYPE_POWER_AVERAGE:
            json_object_object_add(jsonSensor, pSensor->name, json_object_new_string("Average"));
            break;
        default:
            json_object_object_add(jsonSensor, "name", json_object_new_string(pSensor->name));
            break;
    }

    switch (pSensor->type) {
        case NT_SENSOR_TYPE_TEMPERATURE:
            fdivFac = 10.0f;
            json_object_object_add(jsonSensor, "unit", json_object_new_string("Temp. [C]"));
            break;
        case NT_SENSOR_TYPE_VOLTAGE:
            fdivFac = 100.0f;
            json_object_object_add(jsonSensor, "unit", json_object_new_string("Volt. [V]"));
            break;
        case NT_SENSOR_TYPE_CURRENT:
            fdivFac = 1000.0f;
            json_object_object_add(jsonSensor, "unit", json_object_new_string("Curr. [mA]"));
            break;
        case NT_SENSOR_TYPE_POWER:
            fdivFac = 10.0f;
            json_object_object_add(jsonSensor, "unit", json_object_new_string("Power [uW]"));
            break;
        case NT_SENSOR_TYPE_HIGH_POWER:
            fdivFac = 10.0f;
            json_object_object_add(jsonSensor, "unit", json_object_new_string("Power [W]"));
            break;
        case NT_SENSOR_TYPE_FAN:
            fdivFac = 1.0f;
            json_object_object_add(jsonSensor, "unit", json_object_new_string("RPM"));
            break;
        default:
            fdivFac = 1.0f;
            json_object_object_add(jsonSensor, "unit", json_object_new_string("**** ERROR UNKNOWN SENSOR TYPE"));
    }

    json_object_object_add(jsonSensor, "limitLow", json_object_new_double((float) pSensor->limitLow / fdivFac));
    json_object_object_add(jsonSensor, "limitHigh", json_object_new_double((float) pSensor->limitHigh / fdivFac));
    json_object_object_add(jsonSensor, "value", json_object_new_double((float) pSensor->value / fdivFac));
    json_object_object_add(jsonSensor, "state", json_object_new_string(pSensor->state == NT_SENSOR_STATE_ALARM ? "Alarm!" : "OK"));
    json_object_object_add(jsonSensor, "valueLowest", json_object_new_double((float) pSensor->valueLowest / fdivFac));
    json_object_object_add(jsonSensor, "valueHighest", json_object_new_double((float) pSensor->valueHighest / fdivFac));

    //    printf("----->>>  %s  <<-----\n", json_object_to_json_string(jsonSensor));

    return jsonSensor;
}

static struct json_object * getPortSensorInfo(NtInfoStream_t hInfo,
        uint32_t firstPort,
        uint32_t lastPort) {
    int32_t status = NT_SUCCESS;
    NtInfo_t infoSensor;
    uint32_t port, sensor;
    //    int prtPort;
    char errBuf[NT_ERRBUF_SIZE];

    struct json_object *jsonPortSensors = json_object_new_array();
    for (port = firstPort; port < lastPort; port++) {
        NtInfo_t infoPort;
        struct json_object *jsonSensor;

        infoPort.cmd = NT_INFO_CMD_READ_PORT_V7;
        infoPort.u.port_v7.portNo = (uint8_t) (port);
        if ((status = NT_InfoRead(hInfo, &infoPort)) != NT_SUCCESS) {
            NT_ExplainError(status, errBuf, sizeof (errBuf));
            fprintf(stderr, ">>> Error: NT_InfoRead failed (6). Code %d = %s\n", status, errBuf);
            return NULL;
        }

        for (sensor = 0; sensor < (int) infoPort.u.port_v7.data.numSensors; sensor++) {
            infoSensor.cmd = NT_INFO_CMD_READ_SENSOR;
            infoSensor.u.sensor.source = NT_SENSOR_SOURCE_PORT;
            infoSensor.u.sensor.sourceIndex = port;
            infoSensor.u.sensor.sensorIndex = sensor;
            status = NT_InfoRead(hInfo, &infoSensor);

            if (status == NT_SUCCESS) {
                if (opt_sensors || (opt_alarm && infoSensor.u.sensor.data.state == NT_SENSOR_STATE_ALARM)) {
                    jsonSensor = getSingleSensorData(&infoSensor.u.sensor.data);
                    json_object_object_add(jsonSensor, "port", json_object_new_int(port));
                    json_object_array_add(jsonPortSensors, jsonSensor);
                }
            } else {
                NT_ExplainError(status, errBuf, sizeof (errBuf));
            }
        }

        for (sensor = 0; sensor < (int) infoPort.u.port_v7.data.numLevel1Sensors; sensor++) {
            infoSensor.cmd = NT_INFO_CMD_READ_SENSOR;
            infoSensor.u.sensor.source = NT_SENSOR_SOURCE_LEVEL1_PORT;
            infoSensor.u.sensor.sourceIndex = port;
            infoSensor.u.sensor.sensorIndex = sensor;
            status = NT_InfoRead(hInfo, &infoSensor);

            if (status == NT_SUCCESS) {
                if (opt_sensors || (opt_alarm && infoSensor.u.sensor.data.state == NT_SENSOR_STATE_ALARM)) {
                    jsonSensor = getSingleSensorData(&infoSensor.u.sensor.data);
                    json_object_object_add(jsonSensor, "port", json_object_new_int(port));
                    json_object_array_add(jsonPortSensors, jsonSensor);
                }
            } else {
                NT_ExplainError(status, errBuf, sizeof (errBuf));
            }
        }
    }

    struct json_object *jsonPortSensorsContainer = json_object_new_object();

    json_object_object_add(jsonPortSensorsContainer, "PortSensors", jsonPortSensors);

    return jsonPortSensorsContainer;
}

static struct json_object * getSensorInfo(int adapter, NtInfoStream_t hInfo,
        uint32_t numSensors) {
    int32_t status = NT_SUCCESS;
    NtInfo_t infoSensor;
    uint32_t sensorCounter;
    char errBuf[NT_ERRBUF_SIZE];

    struct json_object *jsonSensors = json_object_new_array();

    for (sensorCounter = 0; sensorCounter < numSensors; sensorCounter++) {
        struct json_object *jsonSensor = NULL;

        infoSensor.cmd = NT_INFO_CMD_READ_SENSOR;
        infoSensor.u.sensor.source = NT_SENSOR_SOURCE_ADAPTER;
        infoSensor.u.sensor.sourceIndex = adapter;
        infoSensor.u.sensor.sensorIndex = sensorCounter;
        status = NT_InfoRead(hInfo, &infoSensor);

        if (status == NT_SUCCESS) {
            if (opt_sensors || (opt_alarm && infoSensor.u.sensor.data.state == NT_SENSOR_STATE_ALARM)) {
                jsonSensor = getSingleSensorData(&infoSensor.u.sensor.data);
            }
            //printf("\n--- getSingleSensorData --->>>\n%s\n<<<-----\n", json_object_to_json_string_ext(jsonSensor, JSON_C_TO_STRING_PRETTY));
        } else {
            NT_ExplainError(status, errBuf, sizeof (errBuf));
            fprintf(stderr, ">>> Error: NT_InfoRead failed (5). Code %d = %s\n", status, errBuf);
            return NULL;
        }

        if (jsonSensor) {
            json_object_array_add(jsonSensors, jsonSensor);
        }
        //printf("\n--- jsonSensors --->>>\n%s\n<<<-----\n", json_object_to_json_string_ext(jsonSensors, JSON_C_TO_STRING_PRETTY));
    }

    //printf("\n----->>>\n%s\n<<<-----\n", json_object_to_json_string_ext(jsonSensors, JSON_C_TO_STRING_PRETTY));

    return jsonSensors;
}

static struct json_object *getAdapterInfo(NtInfoStream_t hInfo, uint32_t adapter, uint32_t *numSensors, uint32_t *firstPort, uint32_t *lastPort) {
    int32_t status = NT_SUCCESS;
    NtInfo_t infoAdapter;
    char buffer[NT_ERRBUF_SIZE];

    struct json_object *jsonAdapter = json_object_new_object();

    infoAdapter.cmd = NT_INFO_CMD_READ_ADAPTER_V6;
    infoAdapter.u.adapter_v6.adapterNo = (uint8_t) adapter;

    if ((status = NT_InfoRead(hInfo, &infoAdapter)) != NT_SUCCESS) {
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, ">>> Error: NT_InfoRead failed (8). Code %d = %s\n", status, buffer);
        return NULL;
    }

    json_object_object_add(jsonAdapter, "id", json_object_new_int((int) infoAdapter.u.adapter_v6.adapterNo));
    json_object_object_add(jsonAdapter, "name", json_object_new_string(infoAdapter.u.adapter_v6.data.name));

    struct json_object *jsonProdInfo = NULL;
    if (opt_product) {
        jsonProdInfo = getProductInfo(hInfo, adapter);
        json_object_object_add(jsonAdapter, "ProductInfo", jsonProdInfo);
    }

    *numSensors = infoAdapter.u.adapter_v6.data.numSensors;
    *firstPort = infoAdapter.u.adapter_v6.data.portOffset;
    *lastPort = infoAdapter.u.adapter_v6.data.portOffset + infoAdapter.u.adapter_v6.data.numPorts;

    return jsonAdapter;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
static struct json_object * getTimeSyncStatExt(NtInfoStream_t hInfo, uint32_t adapter) {
    int32_t status = NT_SUCCESS;
    NtInfo_t infoAdapter;
    char buffer[NT_ERRBUF_SIZE];

    struct json_object *jsonTimeSyncStatExt = json_object_new_object();

    infoAdapter.cmd = NT_INFO_CMD_READ_TIMESYNC_STATUS_EXT;
    infoAdapter.u.timeSyncExt.adapterNo = (uint8_t) adapter;

    if ((status = NT_InfoRead(hInfo, &infoAdapter)) != NT_SUCCESS) {
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, ">>> Error: NT_InfoRead failed (8). Code %d = %s\n", status, buffer);
        return NULL;
    }
    json_object_object_add(jsonTimeSyncStatExt, "valid", json_object_new_int(infoAdapter.u.timeSyncExt.data.infoValid));

    if (infoAdapter.u.timeSyncExt.data.infoValid) {
        json_object_object_add(jsonTimeSyncStatExt, "extDevSyncSignalOk", json_object_new_int(infoAdapter.u.timeSyncExt.data.extDevSyncSignalOk));
        json_object_object_add(jsonTimeSyncStatExt, "extDevInSync", json_object_new_int(infoAdapter.u.timeSyncExt.data.extDevInSync));
        json_object_object_add(jsonTimeSyncStatExt, "extDevTimeOfDayOk", json_object_new_int(infoAdapter.u.timeSyncExt.data.extDevTimeOfDayOk));
        json_object_object_add(jsonTimeSyncStatExt, "extDevOsModeSyncEnabled", json_object_new_int(infoAdapter.u.timeSyncExt.data.extDevOsModeSyncEnabled));
        json_object_object_add(jsonTimeSyncStatExt, "extDevOsModeInSync", json_object_new_int(infoAdapter.u.timeSyncExt.data.extDevOsModeInSync));
        json_object_object_add(jsonTimeSyncStatExt, "extDevIsMaster", json_object_new_int(infoAdapter.u.timeSyncExt.data.extDevIsMaster));
        json_object_object_add(jsonTimeSyncStatExt, "extAdapterIsMaster", json_object_new_int(infoAdapter.u.timeSyncExt.data.extAdapterIsMaster));
        json_object_object_add(jsonTimeSyncStatExt, "extDevMasterID", json_object_new_int(infoAdapter.u.timeSyncExt.data.extDevMasterID));

    }
    return jsonTimeSyncStatExt;
}

static char * decodeTimeSyncConnector(enum NtTimeSyncConnectorSetting_e connSetting, char *buffer) {

    switch (connSetting) {
        case NT_TIMESYNC_CONNECTOR_SETTING_NONE: snprintf(buffer, NT_ERRBUF_SIZE, "NONE");
            break;
        case NT_TIMESYNC_CONNECTOR_SETTING_NTTS_IN: snprintf(buffer, NT_ERRBUF_SIZE, "NTTS_IN");
            break;
        case NT_TIMESYNC_CONNECTOR_SETTING_PPS_IN: snprintf(buffer, NT_ERRBUF_SIZE, "PPS_IN");
            break;
        case NT_TIMESYNC_CONNECTOR_SETTING_10MPPS_IN: snprintf(buffer, NT_ERRBUF_SIZE, "10MPPS_IN");
            break;
        case NT_TIMESYNC_CONNECTOR_SETTING_NTTS_OUT: snprintf(buffer, NT_ERRBUF_SIZE, "NTTS_OUT");
            break;
        case NT_TIMESYNC_CONNECTOR_SETTING_PPS_OUT: snprintf(buffer, NT_ERRBUF_SIZE, "PPS_OUT");
            break;
        case NT_TIMESYNC_CONNECTOR_SETTING_10MPPS_OUT: snprintf(buffer, NT_ERRBUF_SIZE, "10MPPS_OUT");
            break;
        case NT_TIMESYNC_CONNECTOR_SETTING_REPEAT_INT1: snprintf(buffer, NT_ERRBUF_SIZE, "REPEAT_INT1");
            break;
        case NT_TIMESYNC_CONNECTOR_SETTING_REPEAT_INT2: snprintf(buffer, NT_ERRBUF_SIZE, "REPEAT_INT2");
            break;
        case NT_TIMESYNC_CONNECTOR_SETTING_REPEAT_EXT1: snprintf(buffer, NT_ERRBUF_SIZE, "REPEAT_EXT1");
            break;
        default: snprintf(buffer, NT_ERRBUF_SIZE, "Error");
    }
    return buffer;
};

static char *decodeTimeSyncRef(enum NtTimeSyncReference_e tr, char *buffer) {

    switch (tr) {
        case NT_TIMESYNC_REFERENCE_INVALID: //snprintf(buffer, NT_ERRBUF_SIZE, "%s %s", buffer, "");
            break;
        case NT_TIMESYNC_REFERENCE_FREE_RUN: snprintf(buffer, NT_ERRBUF_SIZE, "FREE_RUN");
            break;
        case NT_TIMESYNC_REFERENCE_PTP: snprintf(buffer, NT_ERRBUF_SIZE, "PTP");
            break;
        case NT_TIMESYNC_REFERENCE_INT1: snprintf(buffer, NT_ERRBUF_SIZE, "INT1");
            break;
        case NT_TIMESYNC_REFERENCE_INT2: snprintf(buffer, NT_ERRBUF_SIZE, "INT2");
            break;
        case NT_TIMESYNC_REFERENCE_EXT1: snprintf(buffer, NT_ERRBUF_SIZE, "EXT1");
            break;
        case NT_TIMESYNC_REFERENCE_OSTIME: snprintf(buffer, NT_ERRBUF_SIZE, "OSTIME");
            break;
    }
    return buffer;
}

static char *decodeTimeSyncRefPriority(enum NtTimeSyncReference_e *tsrp, char *buffer) {
    int i;
    snprintf(buffer, NT_ERRBUF_SIZE, " ");

    for (i = 0; i < 4; ++i) {

        if (i != 0 && (tsrp[i] != NT_TIMESYNC_REFERENCE_INVALID)) {
            snprintf(buffer, NT_ERRBUF_SIZE, "%s;", buffer);
        }

        switch (tsrp[i]) {
            case NT_TIMESYNC_REFERENCE_INVALID: //snprintf(buffer, NT_ERRBUF_SIZE, "%s %s", buffer, "");
                break;
            case NT_TIMESYNC_REFERENCE_FREE_RUN: snprintf(buffer, NT_ERRBUF_SIZE, "%s%s", buffer, "FREE_RUN");
                break;
            case NT_TIMESYNC_REFERENCE_PTP: snprintf(buffer, NT_ERRBUF_SIZE, "%s%s", buffer, "PTP");
                break;
            case NT_TIMESYNC_REFERENCE_INT1: snprintf(buffer, NT_ERRBUF_SIZE, "%s%s", buffer, "INT1");
                break;
            case NT_TIMESYNC_REFERENCE_INT2: snprintf(buffer, NT_ERRBUF_SIZE, "%s%s", buffer, "INT2");
                break;
            case NT_TIMESYNC_REFERENCE_EXT1: snprintf(buffer, NT_ERRBUF_SIZE, "%s%s", buffer, "EXT1");
                break;
            case NT_TIMESYNC_REFERENCE_OSTIME: snprintf(buffer, NT_ERRBUF_SIZE, "%s%s", buffer, "OSTIME");
                break;
        };
    }
    return buffer;
}

static char *decodeTimeSyncFreqRef(enum NtTimeSyncFreqReference_e tr, char *buffer) {

    switch (tr) {
        case NT_TIMESYNC_FREQ_REFERENCE_INVALID: //snprintf(buffer, NT_ERRBUF_SIZE, "%s %s", buffer, "");
            break;
        case NT_TIMESYNC_FREQ_REFERENCE_FREE_RUN: snprintf(buffer, NT_ERRBUF_SIZE, "FREE_RUN");
            break;
        case NT_TIMESYNC_FREQ_REFERENCE_SYNC_E: snprintf(buffer, NT_ERRBUF_SIZE, "SYNC_E");
            break;
    }
    return buffer;
}

static char *decodeTimeSyncFreqRefPriority(enum NtTimeSyncFreqReference_e *tsrp, char *buffer) {
    int i;
    snprintf(buffer, NT_ERRBUF_SIZE, " ");

    for (i = 0; i < 4; ++i) {

        if (i != 0 && (tsrp[i] != NT_TIMESYNC_FREQ_REFERENCE_INVALID)) {
            snprintf(buffer, NT_ERRBUF_SIZE, "%s;", buffer);
        }

        switch (tsrp[i]) {
            case NT_TIMESYNC_FREQ_REFERENCE_INVALID: //snprintf(buffer, NT_ERRBUF_SIZE, "%s %s", buffer, "");
                break;
            case NT_TIMESYNC_FREQ_REFERENCE_FREE_RUN: snprintf(buffer, NT_ERRBUF_SIZE, "%s%s", buffer, "FREE_RUN");
                break;
            case NT_TIMESYNC_FREQ_REFERENCE_SYNC_E: snprintf(buffer, NT_ERRBUF_SIZE, "%s%s", buffer, "SYNC_E");
                break;
        };
    }
    return buffer;
}

static struct json_object * getTimeSyncInfo(NtInfoStream_t hInfo, uint32_t adapter) {
    int32_t status = NT_SUCCESS;
    NtInfo_t infoAdapter;
    char buffer[NT_ERRBUF_SIZE];

    struct json_object *jsonTimeSyncInfo = json_object_new_object();

    infoAdapter.cmd = NT_INFO_CMD_READ_TIMESYNC_V4;
    infoAdapter.u.timeSync_v4.adapterNo = (uint8_t) adapter;

    if ((status = NT_InfoRead(hInfo, &infoAdapter)) != NT_SUCCESS) {
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, ">>> Error: NT_InfoRead failed (8). Code %d = %s\n", status, buffer);
        return NULL;
    }

    json_object_object_add(jsonTimeSyncInfo, "timeSyncSupported", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncSupported));
    json_object_object_add(jsonTimeSyncInfo, "ptpSupported", json_object_new_int(infoAdapter.u.timeSync_v4.data.ptpSupported));

    json_object_object_add(jsonTimeSyncInfo, "timeSyncConnectorExt1",
            json_object_new_string(decodeTimeSyncConnector(infoAdapter.u.timeSync_v4.data.timeSyncConnectorExt1, buffer)));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncConnectorInt1",
            json_object_new_string(decodeTimeSyncConnector(infoAdapter.u.timeSync_v4.data.timeSyncConnectorInt1, buffer)));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncConnectorInt2",
            json_object_new_string(decodeTimeSyncConnector(infoAdapter.u.timeSync_v4.data.timeSyncConnectorInt1, buffer)));

    json_object_object_add(jsonTimeSyncInfo, "tsRefPrio",
            json_object_new_string(decodeTimeSyncRefPriority(infoAdapter.u.timeSync_v4.data.tsRefPrio, buffer)));

    json_object_object_add(jsonTimeSyncInfo, "timeRef",
            json_object_new_string(decodeTimeSyncRef(infoAdapter.u.timeSync_v4.data.timeRef, buffer)));


    json_object_object_add(jsonTimeSyncInfo, "tsFreqRefPrio",
            json_object_new_string(decodeTimeSyncFreqRefPriority(infoAdapter.u.timeSync_v4.data.tsFreqRefPrio, buffer)));

    json_object_object_add(jsonTimeSyncInfo, "freqRef",
            json_object_new_string(decodeTimeSyncFreqRef(infoAdapter.u.timeSync_v4.data.freqRef, buffer)));

    json_object_object_add(jsonTimeSyncInfo, "timeSyncNTTSInSyncLimit", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncNTTSInSyncLimit));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncOSInSyncLimit", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncOSInSyncLimit));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncPPSInSyncLimit", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncPPSInSyncLimit));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncPTPInSyncLimit", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncPTPInSyncLimit));


    json_object_object_add(jsonTimeSyncInfo, "timeSyncInSyncStatus",
            json_object_new_string(
            infoAdapter.u.timeSync_v4.data.timeSyncInSyncStatus == NT_TIMESYNC_INSYNC_STATUS_NONE ? "None"
            : infoAdapter.u.timeSync_v4.data.timeSyncInSyncStatus == NT_TIMESYNC_INSYNC_STATUS_IN_SYNC ? "IN_SYNC"
            : infoAdapter.u.timeSync_v4.data.timeSyncInSyncStatus == NT_TIMESYNC_INSYNC_STATUS_NOT_IN_SYNC ? "NON_IN_SYNC"
            : "Error"
            ));

    json_object_object_add(jsonTimeSyncInfo, "timeSyncCurrentConStatus",
            json_object_new_string(
            infoAdapter.u.timeSync_v4.data.timeSyncCurrentConStatus == NT_TIMESYNC_CONNECTOR_STATUS_NONE ? "None"
            : infoAdapter.u.timeSync_v4.data.timeSyncCurrentConStatus == NT_TIMESYNC_CONNECTOR_STATUS_SIGNAL_LOST ? "SIGNAL_LOST"
            : infoAdapter.u.timeSync_v4.data.timeSyncCurrentConStatus == NT_TIMESYNC_CONNECTOR_STATUS_SIGNAL_PRESENT ? "SIGNAL_PRESENT"
            : "Error"
            ));


    json_object_object_add(jsonTimeSyncInfo, "timeSyncPpsEnable",
            json_object_new_string(
            infoAdapter.u.timeSync_v4.data.timeSyncPpsEnable == NT_TIMESYNC_PPS_STATUS_NONE ? "None"
            : infoAdapter.u.timeSync_v4.data.timeSyncPpsEnable == NT_TIMESYNC_PPS_STATUS_ENABLED ? "ENABLED"
            : infoAdapter.u.timeSync_v4.data.timeSyncPpsEnable == NT_TIMESYNC_PPS_STATUS_DISABLED ? "DISABLED"
            : "Error"
            ));


    json_object_object_add(jsonTimeSyncInfo, "timeSyncPpsSyncMode",
            json_object_new_string(
            infoAdapter.u.timeSync_v4.data.timeSyncPpsSyncMode == NT_TIMESYNC_PPS_MODE_NOT_SYNCING ? "NOT_SYNCING"
            : infoAdapter.u.timeSync_v4.data.timeSyncPpsSyncMode == NT_TIMESYNC_PPS_MODE_PHASE_SYNCING ? "PHASE_SYNCING"
            : infoAdapter.u.timeSync_v4.data.timeSyncPpsSyncMode == NT_TIMESYNC_PPS_MODE_TIME_SYNCING ? "MODE_TIME_SYNCING "
            : "Error"
            ));

    json_object_object_add(jsonTimeSyncInfo, "timeSyncClockAdjustmentMode", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncClockAdjustmentMode));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncHardReset", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncHardReset));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncTimeJumpThreshold", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncTimeJumpThreshold));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncTimeOffset", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncTimeOffset));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncPpsSampled", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncPpsSampled));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncTimeSkew", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncTimeSkew));
    json_object_object_add(jsonTimeSyncInfo, "highFrequencySampling", json_object_new_int(infoAdapter.u.timeSync_v4.data.highFrequencySampling));

    // to do:
    //  struct NtInfoTimeSyncSample_s sample[NT_TIMESYNC_SAMPLING_CNT]; //!< Timestamp sample sets. @ref NtTimeSyncSamplingSrc_e

    json_object_object_add(jsonTimeSyncInfo, "timeSyncAdapterToOSSyncEnabled", json_object_new_int(infoAdapter.u.timeSync_v4.data.timeSyncAdapterToOSSyncEnabled));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncOSClockOffset", json_object_new_int64(infoAdapter.u.timeSync_v4.data.timeSyncOSClockOffset));
    json_object_object_add(jsonTimeSyncInfo, "timeSyncOSClockRateAdjustment", json_object_new_int64(infoAdapter.u.timeSync_v4.data.timeSyncOSClockRateAdjustment));

    return jsonTimeSyncInfo;
}

static struct json_object * getTimeSyncStat(NtInfoStream_t hInfo, uint32_t adapter) {
    int32_t status = NT_SUCCESS;
    NtInfo_t infoAdapter;
    char buffer[NT_ERRBUF_SIZE];

    struct json_object *jsonTimeStat = json_object_new_object();

    infoAdapter.cmd = NT_INFO_CMD_READ_TIMESYNC_STAT;
    infoAdapter.u.timeSyncStat.adapterNo = (uint8_t) adapter;

    if ((status = NT_InfoRead(hInfo, &infoAdapter)) != NT_SUCCESS) {
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, ">>> Error: NT_InfoRead failed (8). Code %d = %s\n", status, buffer);
        return NULL;
    }
    json_object_object_add(jsonTimeStat, "supported",
            json_object_new_string(
            infoAdapter.u.timeSyncStat.data.supported == NT_TIMESYNC_STATISTICS_NO_SUPPORT ? "NO_SUPPORT"
            : infoAdapter.u.timeSyncStat.data.supported == NT_TIMESYNC_STATISTICS_PTP_ONLY ? "PTP_ONLY"
            : infoAdapter.u.timeSyncStat.data.supported == NT_TIMESYNC_STATISTICS_FULL_SUPPORT ? "FULL_SUPPORT"
            : "Error."
            ));

    json_object_object_add(jsonTimeStat, "samples", json_object_new_int64(infoAdapter.u.timeSyncStat.data.samples));
    json_object_object_add(jsonTimeStat, "skew", json_object_new_int64(infoAdapter.u.timeSyncStat.data.skew));
    json_object_object_add(jsonTimeStat, "min", json_object_new_int64(infoAdapter.u.timeSyncStat.data.min));
    json_object_object_add(jsonTimeStat, "max", json_object_new_int64(infoAdapter.u.timeSyncStat.data.max));
    json_object_object_add(jsonTimeStat, "mean", json_object_new_int64(infoAdapter.u.timeSyncStat.data.mean));
    json_object_object_add(jsonTimeStat, "jitter", json_object_new_int64(infoAdapter.u.timeSyncStat.data.jitter));
    json_object_object_add(jsonTimeStat, "secSinceReset", json_object_new_int64(infoAdapter.u.timeSyncStat.data.secSinceReset));

    json_object_object_add(jsonTimeStat, "stdDevSqr", json_object_new_double(infoAdapter.u.timeSyncStat.data.stdDevSqr));

    json_object_object_add(jsonTimeStat, "signalLostCnt", json_object_new_int64(infoAdapter.u.timeSyncStat.data.signalLostCnt));
    json_object_object_add(jsonTimeStat, "syncLostCnt", json_object_new_int64(infoAdapter.u.timeSyncStat.data.syncLostCnt));
    json_object_object_add(jsonTimeStat, "hardResetCnt", json_object_new_int64(infoAdapter.u.timeSyncStat.data.hardResetCnt));


    return jsonTimeStat;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
static void portSummary2JSON(struct json_object *jsonPortStats, struct NtPortStatistics_v1_s portStats) {

    //    struct json_object *jsonRMON1 = json_object_new_object();
    if (portStats.valid.RMON1) {
        json_object_object_add(jsonPortStats, "pkts", json_object_new_int64(portStats.RMON1.pkts));
        json_object_object_add(jsonPortStats, "octets", json_object_new_int64(portStats.RMON1.octets));
        json_object_object_add(jsonPortStats, "pkts_drop", json_object_new_int64(portStats.RMON1.dropEvents));

        json_object_object_add(jsonPortStats, "pkts_oversize", json_object_new_int64(portStats.RMON1.oversizePkts));
        json_object_object_add(jsonPortStats, "pkts_undersize", json_object_new_int64(portStats.RMON1.undersizePkts));
        json_object_object_add(jsonPortStats, "pkts_multicast", json_object_new_int64(portStats.RMON1.multicastPkts));
        json_object_object_add(jsonPortStats, "pkts_jabber", json_object_new_int64(portStats.RMON1.jabbers));
        json_object_object_add(jsonPortStats, "pkts_fragments", json_object_new_int64(portStats.RMON1.fragments));
        json_object_object_add(jsonPortStats, "pkts_collisions", json_object_new_int64(portStats.RMON1.collisions));
        json_object_object_add(jsonPortStats, "pkts_crcAlignErrors", json_object_new_int64(portStats.RMON1.crcAlignErrors));
        json_object_object_add(jsonPortStats, "pkts_broadcast", json_object_new_int64(portStats.RMON1.broadcastPkts));

    }

    if (portStats.valid.extDrop) {
        json_object_object_add(jsonPortStats, "pkts_drop_Dedup", json_object_new_int64(portStats.extDrop.pktsDedup));
        json_object_object_add(jsonPortStats, "pkts_drop_NoFilter", json_object_new_int64(portStats.extDrop.pktsNoFilter));
        json_object_object_add(jsonPortStats, "pkts_drop_Overflow", json_object_new_int64(portStats.extDrop.pktsOverflow));
        json_object_object_add(jsonPortStats, "pkts_drop_MacBandwidth", json_object_new_int64(portStats.extDrop.pktsMacBandwidth));
    }
}

static void portStats2JSON(struct json_object *jsonPortStats, struct NtPortStatistics_v1_s portStats) {
    if (portStats.valid.RMON1) {
        struct json_object *jsonRMON1 = json_object_new_object();
        json_object_object_add(jsonRMON1, "pkts", json_object_new_int64(portStats.RMON1.pkts));
        json_object_object_add(jsonRMON1, "octets", json_object_new_int64(portStats.RMON1.octets));
        json_object_object_add(jsonRMON1, "pkts_drop", json_object_new_int64(portStats.RMON1.dropEvents));

        json_object_object_add(jsonRMON1, "pkts_64_Octets", json_object_new_int64(portStats.RMON1.pkts64Octets));
        json_object_object_add(jsonRMON1, "pkts_65_127_Octets", json_object_new_int64(portStats.RMON1.pkts65to127Octets));
        json_object_object_add(jsonRMON1, "pkts_128_255_Octets", json_object_new_int64(portStats.RMON1.pkts128to255Octets));
        json_object_object_add(jsonRMON1, "pkts_256_511_Octets", json_object_new_int64(portStats.RMON1.pkts256to511Octets));
        json_object_object_add(jsonRMON1, "pkts_512_1023_Octets", json_object_new_int64(portStats.RMON1.pkts512to1023Octets));
        json_object_object_add(jsonRMON1, "pkts_1024_1518_Octets", json_object_new_int64(portStats.RMON1.pkts1024to1518Octets));

        json_object_object_add(jsonRMON1, "pkts_oversize", json_object_new_int64(portStats.RMON1.oversizePkts));
        json_object_object_add(jsonRMON1, "pkts_undersize", json_object_new_int64(portStats.RMON1.undersizePkts));
        json_object_object_add(jsonRMON1, "pkts_multicast", json_object_new_int64(portStats.RMON1.multicastPkts));
        json_object_object_add(jsonRMON1, "pkts_jabber", json_object_new_int64(portStats.RMON1.jabbers));
        json_object_object_add(jsonRMON1, "pkts_fragments", json_object_new_int64(portStats.RMON1.fragments));
        json_object_object_add(jsonRMON1, "pkts_collisions", json_object_new_int64(portStats.RMON1.collisions));
        json_object_object_add(jsonRMON1, "pkts_crcAlignErrors", json_object_new_int64(portStats.RMON1.crcAlignErrors));
        json_object_object_add(jsonRMON1, "pkts_broadcast", json_object_new_int64(portStats.RMON1.broadcastPkts));
        json_object_object_add(jsonPortStats, "RMON", jsonRMON1);
    }

    if (portStats.valid.extRMON) {
        struct json_object *jsonExtRMON = json_object_new_object();
        json_object_object_add(jsonExtRMON, "pkts_1519to2047_Octets", json_object_new_int64(portStats.extRMON.pkts1519to2047Octets));
        json_object_object_add(jsonExtRMON, "pkts_2048to4095_Octets", json_object_new_int64(portStats.extRMON.pkts2048to4095Octets));
        json_object_object_add(jsonExtRMON, "pkts_4096to8191_Octets", json_object_new_int64(portStats.extRMON.pkts4096to8191Octets));
        json_object_object_add(jsonExtRMON, "pkts_8192toMax_Octets", json_object_new_int64(portStats.extRMON.pkts8192toMaxOctets));
        json_object_object_add(jsonExtRMON, "pkts_Alignment", json_object_new_int64(portStats.extRMON.pktsAlignment));
        json_object_object_add(jsonExtRMON, "pkts_CodeViolation", json_object_new_int64(portStats.extRMON.pktsCodeViolation));
        json_object_object_add(jsonExtRMON, "pkts_Crc", json_object_new_int64(portStats.extRMON.pktsCrc));
        json_object_object_add(jsonExtRMON, "pkts_HardSlice", json_object_new_int64(portStats.extRMON.pktsHardSlice));
        json_object_object_add(jsonExtRMON, "pkts_HardSliceJabber", json_object_new_int64(portStats.extRMON.pktsHardSliceJabber));
        json_object_object_add(jsonExtRMON, "pkts_unicast", json_object_new_int64(portStats.extRMON.unicastPkts));
        json_object_object_add(jsonPortStats, "ExtRMON", jsonExtRMON);
    }

    if (portStats.valid.extDrop) {
        struct json_object *jsonExtDrop = json_object_new_object();
        json_object_object_add(jsonExtDrop, "pkts_Dedup", json_object_new_int64(portStats.extDrop.pktsDedup));
        json_object_object_add(jsonExtDrop, "octets_Dedup", json_object_new_int64(portStats.extDrop.octetsDedup));
        json_object_object_add(jsonExtDrop, "pkts_NoFilter", json_object_new_int64(portStats.extDrop.pktsNoFilter));
        json_object_object_add(jsonExtDrop, "octets_NoFilter", json_object_new_int64(portStats.extDrop.octetsNoFilter));
        json_object_object_add(jsonExtDrop, "pkts_Overflow", json_object_new_int64(portStats.extDrop.pktsOverflow));
        json_object_object_add(jsonExtDrop, "octets_Overflow", json_object_new_int64(portStats.extDrop.octetsOverflow));
        json_object_object_add(jsonExtDrop, "pkts_MacBandwidth", json_object_new_int64(portStats.extDrop.pktsMacBandwidth));
        json_object_object_add(jsonPortStats, "ExtDrop", jsonExtDrop);
    }

    if (portStats.valid.decode) {
        struct json_object *jsonDecode = json_object_new_object();
        json_object_object_add(jsonDecode, "pkts_BabyGiant", json_object_new_int64(portStats.decode.pktsBabyGiant));
        json_object_object_add(jsonDecode, "pkts_GiantUndersize", json_object_new_int64(portStats.decode.pktsGiantUndersize));
        json_object_object_add(jsonDecode, "pkts_Isl", json_object_new_int64(portStats.decode.pktsIsl));
        json_object_object_add(jsonDecode, "pkts_IslMpls", json_object_new_int64(portStats.decode.pktsIslMpls));
        json_object_object_add(jsonDecode, "pkts_IslVlan", json_object_new_int64(portStats.decode.pktsIslVlan));
        json_object_object_add(jsonDecode, "pkts_IslVlanMpls", json_object_new_int64(portStats.decode.pktsIslVlanMpls));
        json_object_object_add(jsonDecode, "pkts_Mpls", json_object_new_int64(portStats.decode.pktsMpls));
        json_object_object_add(jsonDecode, "pkts_NotIslVlanMpls", json_object_new_int64(portStats.decode.pktsNotIslVlanMpls));
        json_object_object_add(jsonDecode, "pkts_Vlan", json_object_new_int64(portStats.decode.pktsVlan));
        json_object_object_add(jsonDecode, "pkts_VlanMpls", json_object_new_int64(portStats.decode.pktsVlanMpls));
        json_object_object_add(jsonPortStats, "Decode", jsonDecode);
    }
    if (portStats.valid.chksum) {
        struct json_object *jsonChecksum = json_object_new_object();
        json_object_object_add(jsonChecksum, "pkts_IpChkSumError", json_object_new_int64(portStats.chksum.pktsIpChkSumError));
        json_object_object_add(jsonChecksum, "pkts_TcpChkSumError", json_object_new_int64(portStats.chksum.pktsTcpChkSumError));
        json_object_object_add(jsonChecksum, "pkts_UdpChkSumError", json_object_new_int64(portStats.chksum.pktsUdpChkSumError));
        json_object_object_add(jsonPortStats, "Checksum", jsonChecksum);
    }

    if (portStats.valid.ipf) {
        struct json_object *jsonIpFrag = json_object_new_object();
        json_object_object_add(jsonIpFrag, "pkts_FragTableFirstHit", json_object_new_int64(portStats.ipf.ipFragTableFirstHit));
        json_object_object_add(jsonIpFrag, "pkts_FragTableFirstNoHit", json_object_new_int64(portStats.ipf.ipFragTableFirstNoHit));
        json_object_object_add(jsonIpFrag, "pkts_FragTableLastHit", json_object_new_int64(portStats.ipf.ipFragTableLastHit));
        json_object_object_add(jsonIpFrag, "pkts_FragTableLastNoHit", json_object_new_int64(portStats.ipf.ipFragTableLastNoHit));
        json_object_object_add(jsonIpFrag, "pkts_FragTableMidHit", json_object_new_int64(portStats.ipf.ipFragTableMidHit));
        json_object_object_add(jsonIpFrag, "pkts_FragTableMidNoHit", json_object_new_int64(portStats.ipf.ipFragTableMidNoHit));
        json_object_object_add(jsonPortStats, "IpFrag", jsonIpFrag);
    }
}

static char *decodeSpeed(enum NtLinkSpeed_e speed, char *buffer) {
    switch (speed) {
        case NT_LINK_SPEED_UNKNOWN: snprintf(buffer, NT_ERRBUF_SIZE, "UNKNOWN");
            break;
        case NT_LINK_SPEED_10M: snprintf(buffer, NT_ERRBUF_SIZE, "0.01");
            break;
        case NT_LINK_SPEED_100M: snprintf(buffer, NT_ERRBUF_SIZE, "0.1");
            break;
        case NT_LINK_SPEED_1G: snprintf(buffer, NT_ERRBUF_SIZE, "1");
            break;
        case NT_LINK_SPEED_10G: snprintf(buffer, NT_ERRBUF_SIZE, "10");
            break;
        case NT_LINK_SPEED_40G: snprintf(buffer, NT_ERRBUF_SIZE, "40");
            break;
        case NT_LINK_SPEED_100G: snprintf(buffer, NT_ERRBUF_SIZE, "100");
            break;
        default: snprintf(buffer, NT_ERRBUF_SIZE, "ERROR");
    }
    return buffer;
}

static char *decodeState(enum NtLinkState_e state, char *buffer) {
    switch (state) {
        case NT_LINK_STATE_UNKNOWN: snprintf(buffer, NT_ERRBUF_SIZE, "UNKNOWN");
            break;
        case NT_LINK_STATE_DOWN: snprintf(buffer, NT_ERRBUF_SIZE, "DOWN");
            break;
        case NT_LINK_STATE_UP: snprintf(buffer, NT_ERRBUF_SIZE, "UP");
            break;
        case NT_LINK_STATE_ERROR: snprintf(buffer, NT_ERRBUF_SIZE, "ERROR");
            break;
        default: snprintf(buffer, NT_ERRBUF_SIZE, "READ_ERROR");
    }
    return buffer;
}

static char *decodePortType(enum NtPortType_e type, char *buffer) {
    switch (type) {
        case NT_PORT_TYPE_NOT_AVAILABLE: snprintf(buffer, NT_ERRBUF_SIZE, "NOT_AVAILABLE");
            break;
        case NT_PORT_TYPE_NOT_RECOGNISED: snprintf(buffer, NT_ERRBUF_SIZE, "NOT_RECOGNISED");
            break;
        case NT_PORT_TYPE_RJ45: snprintf(buffer, NT_ERRBUF_SIZE, "RJ45");
            break;
        case NT_PORT_TYPE_SFP_NOT_PRESENT: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_NOT_PRESENT");
            break;
        case NT_PORT_TYPE_SFP_SX: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_SX");
            break;
        case NT_PORT_TYPE_SFP_SX_DD: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_SX_DD");
            break;
        case NT_PORT_TYPE_SFP_LX: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_LX");
            break;
        case NT_PORT_TYPE_SFP_LX_DD: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_LX_DD");
            break;
        case NT_PORT_TYPE_SFP_ZX: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_ZX");
            break;
        case NT_PORT_TYPE_SFP_ZX_DD: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_ZX_DD");
            break;
        case NT_PORT_TYPE_SFP_CU: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_CU");
            break;
        case NT_PORT_TYPE_SFP_CU_DD: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_CU_DD");
            break;
        case NT_PORT_TYPE_SFP_NOT_RECOGNISED: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_NOT_RECOGNISED");
            break;
        case NT_PORT_TYPE_XFP: snprintf(buffer, NT_ERRBUF_SIZE, "XFP");
            break;
        case NT_PORT_TYPE_XPAK: snprintf(buffer, NT_ERRBUF_SIZE, "XPAK");
            break;
        case NT_PORT_TYPE_SFP_CU_TRI_SPEED: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_CU_TRI_SPEED");
            break;
        case NT_PORT_TYPE_SFP_CU_TRI_SPEED_DD: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_CU_TRI_SPEED_DD");
            break;
        case NT_PORT_TYPE_SFP_PLUS: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_PLUS");
            break;
        case NT_PORT_TYPE_SFP_PLUS_NOT_PRESENT: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_PLUS_NOT_PRESENT");
            break;
        case NT_PORT_TYPE_XFP_NOT_PRESENT: snprintf(buffer, NT_ERRBUF_SIZE, "XFP_NOT_PRESENT");
            break;
        case NT_PORT_TYPE_QSFP_PLUS_NOT_PRESENT: snprintf(buffer, NT_ERRBUF_SIZE, "QSFP_PLUS_NOT_PRESENT");
            break;
        case NT_PORT_TYPE_QSFP_PLUS: snprintf(buffer, NT_ERRBUF_SIZE, "QSFP_PLUS");
            break;
        case NT_PORT_TYPE_SFP_PLUS_PASSIVE_DAC: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_PLUS_PASSIVE_DAC");
            break;
        case NT_PORT_TYPE_SFP_PLUS_ACTIVE_DAC: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_PLUS_ACTIVE_DAC");
            break;
        case NT_PORT_TYPE_CFP4: snprintf(buffer, NT_ERRBUF_SIZE, "CFP4");
            break;
        case NT_PORT_TYPE_CFP4_NOT_PRESENT: snprintf(buffer, NT_ERRBUF_SIZE, "CFP4_NOT_PRESENT");
            break;
        case NT_PORT_TYPE_INITIALIZE: snprintf(buffer, NT_ERRBUF_SIZE, "INITIALIZE");
            break;
        case NT_PORT_TYPE_NIM_NOT_PRESENT: snprintf(buffer, NT_ERRBUF_SIZE, "NIM_NOT_PRESENT");
            break;
        case NT_PORT_TYPE_HCB: snprintf(buffer, NT_ERRBUF_SIZE, "HCB");
            break;
        case NT_PORT_TYPE_NOT_SUPPORTED: snprintf(buffer, NT_ERRBUF_SIZE, "NOT_SUPPORTED");
            break;
        case NT_PORT_TYPE_SFP_PLUS_DUAL_RATE: snprintf(buffer, NT_ERRBUF_SIZE, "SFP_PLUS_DUAL_RATE");
            break;
        default: snprintf(buffer, NT_ERRBUF_SIZE, "ERROR");
    }
    return buffer;
}

static char *decodeDuplex(enum NtLinkDuplex_e duplex, char *buffer) {
    switch (duplex) {
        case NT_LINK_DUPLEX_UNKNOWN: snprintf(buffer, NT_ERRBUF_SIZE, "UNKNOWN");
            break;
        case NT_LINK_DUPLEX_HALF: snprintf(buffer, NT_ERRBUF_SIZE, "HALF");
            break;
        case NT_LINK_DUPLEX_FULL: snprintf(buffer, NT_ERRBUF_SIZE, "FULL");
            break;
        default: snprintf(buffer, NT_ERRBUF_SIZE, "ERROR");
    }
    return buffer;
}

static struct json_object * getPortInfo(NtInfoStream_t hInfo) {
    NtInfo_t hInfoSys;
    NtInfo_t hInfoPort;
    char buffer[NT_ERRBUF_SIZE]; // Error buffer
    int status;
    //    int numPorts = 0;
    int port = 0;

    // Read system information to get the number of ports
    //
    hInfoSys.cmd = NT_INFO_CMD_READ_SYSTEM;
    if ((status = NT_InfoRead(hInfo, &hInfoSys)) != 0) {
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, ">>> Error: NT_InfoRead failed (7). Code %d = %s\n", status, buffer);
        return NULL;
    }

    struct json_object *jsonPorts = json_object_new_array();

    for (port = 0; port < hInfoSys.u.system.data.numPorts; ++port) {

        struct json_object *jsonPortInfo = json_object_new_object();
        json_object_object_add(jsonPortInfo, "port", json_object_new_int((int) port));

        // Read the system info
        hInfoPort.cmd = NT_INFO_CMD_READ_PORT_V7;
        hInfoPort.u.port_v7.portNo = port;

        if ((status = NT_InfoRead(hInfo, &hInfoPort)) != NT_SUCCESS) {
            NT_ExplainError(status, buffer, sizeof (buffer) - 1);
            fprintf(stderr, "NT_InfoRead() failed: %s\n", buffer);
        }

        snprintf(buffer, NT_ERRBUF_SIZE, "%02X:%02X:%02X:%02X:%02X:%02X",
                hInfoPort.u.port_v7.data.macAddress[0], hInfoPort.u.port_v7.data.macAddress[1],
                hInfoPort.u.port_v7.data.macAddress[2], hInfoPort.u.port_v7.data.macAddress[3],
                hInfoPort.u.port_v7.data.macAddress[4], hInfoPort.u.port_v7.data.macAddress[5]
                );

        json_object_object_add(jsonPortInfo, "MAC", json_object_new_string(buffer));

        json_object_object_add(jsonPortInfo, "portSpeedGbps",
                json_object_new_string(decodeSpeed(hInfoPort.u.port_v7.data.speed, buffer)));

        json_object_object_add(jsonPortInfo, "linkState",
                json_object_new_string(decodeState(hInfoPort.u.port_v7.data.state, buffer)));

        json_object_object_add(jsonPortInfo, "type",
                json_object_new_string(decodePortType(hInfoPort.u.port_v7.data.type, buffer)));

        json_object_object_add(jsonPortInfo, "duplex",
                json_object_new_string(decodeDuplex(hInfoPort.u.port_v7.data.duplex, buffer)));

        json_object_object_add(jsonPortInfo, "flow_control",
                json_object_new_string(hInfoPort.u.port_v7.data.flow == 1 ? "yes" : "no"));

        json_object_object_add(jsonPortInfo, "maxFrameSize",
                json_object_new_int((int) hInfoPort.u.port_v7.data.maxFrameSize));

        json_object_object_add(jsonPortInfo, "MDI",
                json_object_new_string(hInfoPort.u.port_v7.data.mdi == NT_LINK_MDI_NA ? "NA"
                : hInfoPort.u.port_v7.data.mdi == NT_LINK_MDI_AUTO ? "AUTO"
                : hInfoPort.u.port_v7.data.mdi == NT_LINK_MDI_MDI ? "MDI"
                : hInfoPort.u.port_v7.data.mdi == NT_LINK_MDI_MDIX ? "MDIX"
                : "Unknown"
                ));
        json_object_array_add(jsonPorts, jsonPortInfo);        
    }

    return jsonPorts;
}

static struct json_object * getPortStats(NtInfoStream_t hInfo, NtStatStream_t hStat) {
    NtStatistics_t hStatistics; // Stat handle.
    NtInfo_t hInfoSys;
    char buffer[NT_ERRBUF_SIZE]; // Error buffer
    int status;
    int port;


    // Read the system info
    hInfoSys.cmd = NT_INFO_CMD_READ_SYSTEM;
    if ((status = NT_InfoRead(hInfo, &hInfoSys)) != NT_SUCCESS) {
        NT_ExplainError(status, buffer, sizeof (buffer) - 1);
        fprintf(stderr, "NT_InfoRead() failed: %s\n", buffer);
    }

    // Read the statistics counters to clear the statistics
    // This is an optional step. If omitted, the adapter will show statistics form the start of ntservice.
    hStatistics.cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
    hStatistics.u.query_v2.poll = 0; // Wait for a new set
    hStatistics.u.query_v2.clear = 0; // Clear statistics
    if ((status = NT_StatRead(hStat, &hStatistics)) != NT_SUCCESS) {
        // Get the status code as text
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, "NT_StatRead() failed: %s\n", buffer);
        return NULL;
    }


    struct json_object *jsonPorts = json_object_new_array();

    for (port = 0; port < hInfoSys.u.system.data.numPorts; ++port) {
        hStatistics.cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
        hStatistics.u.query_v2.poll = 1; // The the current counters
        hStatistics.u.query_v2.clear = 0; // Do not clear statistics
        if ((status = NT_StatRead(hStat, &hStatistics)) != NT_SUCCESS) {
            // Get the status code as text
            NT_ExplainError(status, buffer, sizeof (buffer));
            fprintf(stderr, "NT_StatRead() failed: %s\n", buffer);
            return NULL;
        }

        struct json_object *jsonPort = json_object_new_object();
        json_object_object_add(jsonPort, "port", json_object_new_int((int) port));

        if (hStatistics.u.query_v2.data.port.aPorts[port].tsType == NT_TIMESTAMP_TYPE_NATIVE_UNIX) {
            //printf("ports timestamp:  %ld\t\t%s\n", hStatistics.u.query_v2.data.port.aPorts[port].ts, decodeTimeStamp(hStatistics.u.query_v2.data.port.aPorts[port].ts, buffer) );
            json_object_object_add(jsonPort, "ts", json_object_new_int64(hStatistics.u.query_v2.data.port.aPorts[port].ts));
            json_object_object_add(jsonPort, "tsStr", json_object_new_string(decodeTimeStamp(hStatistics.u.query_v2.data.port.aPorts[port].ts, buffer)));
        } else {
            json_object_object_add(jsonPort, "ts", json_object_new_int64(hStatistics.u.query_v2.data.port.aPorts[port].ts));
        }
        json_object_object_add(jsonPort, "tsType",
                json_object_new_string(decodeTimeStampType(hStatistics.u.query_v2.data.port.aPorts[port].tsType, buffer)));


        // Print the RMON1 pkts and octets counters
        if ((!opt_txport) && (hStatistics.u.query_v2.data.port.aPorts[port].rx.valid.RMON1)) {
            struct json_object *jsonRx = json_object_new_object();
            portStats2JSON(jsonRx, hStatistics.u.query_v2.data.port.aPorts[port].rx);
            json_object_object_add(jsonPort, "Rx", jsonRx);

        }
        
        if ((!opt_rxport) && (hStatistics.u.query_v2.data.port.aPorts[port].tx.valid.RMON1)) {

            struct json_object *jsonTx = json_object_new_object();
            portStats2JSON(jsonTx, hStatistics.u.query_v2.data.port.aPorts[port].tx);
            json_object_object_add(jsonPort, "Tx", jsonTx);

        }

        json_object_array_add(jsonPorts, jsonPort);
    }

    return jsonPorts;
}

//-------------------------------------------------------

static uint32_t getPortSummary(
        struct json_object *jsonPorts[],
        uint64_t sysTime,
        NtInfoStream_t hInfo,
        NtStatStream_t hStat) {
    NtInfo_t hInfoSys;
    NtInfo_t hInfoPort;
    char buffer[NT_ERRBUF_SIZE]; // Error buffer
    int status;
    //    int numPorts = 0;
    int port = 0;
    NtStatistics_t hStatistics; // Stat handle.

    // Read system information to get the number of ports
    //
    hInfoSys.cmd = NT_INFO_CMD_READ_SYSTEM;
    if ((status = NT_InfoRead(hInfo, &hInfoSys)) != 0) {
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, ">>> Error: NT_InfoRead failed (7). Code %d = %s\n", status, buffer);
        return 0;
    }

    // Read the statistics counters to clear the statistics
    // This is an optional step. If omitted, the adapter will show statistics form the start of ntservice.
    hStatistics.cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
    hStatistics.u.query_v2.poll = 0; // Wait for a new set
    hStatistics.u.query_v2.clear = 0; // Clear statistics
    if ((status = NT_StatRead(hStat, &hStatistics)) != NT_SUCCESS) {
        // Get the status code as text
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, "NT_StatRead() failed: %s\n", buffer);
        return 0;
    }

    for (port = 0; port < hInfoSys.u.system.data.numPorts; ++port) {

        struct json_object *jsonPortInfo = json_object_new_object();
        json_object_object_add(jsonPortInfo, "port", json_object_new_int((int) port));

        // Read the system info
        hInfoPort.cmd = NT_INFO_CMD_READ_PORT_V6;
        hInfoPort.u.port_v6.portNo = port;

        if ((status = NT_InfoRead(hInfo, &hInfoPort)) != NT_SUCCESS) {
            NT_ExplainError(status, buffer, sizeof (buffer) - 1);
            fprintf(stderr, "NT_InfoRead() failed: %s\n", buffer);
        }

        struct json_object *jsonPort = json_object_new_object();

        //   for (port = 0; port < hInfoSys.u.system.data.numPorts; ++port) {
        hStatistics.cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
        hStatistics.u.query_v2.poll = 1; // The the current counters
        hStatistics.u.query_v2.clear = 0; // Do not clear statistics
        if ((status = NT_StatRead(hStat, &hStatistics)) != NT_SUCCESS) {
            // Get the status code as text
            NT_ExplainError(status, buffer, sizeof (buffer));
            fprintf(stderr, "NT_StatRead() failed: %s\n", buffer);
            return 0;
        }

        json_object_object_add(jsonPort, "port", json_object_new_int((int) port));
        json_object_object_add(jsonPort, "sysTime", json_object_new_int64(sysTime));
        json_object_object_add(jsonPort, "portSpeedGbps",
                json_object_new_string(decodeSpeed(hInfoPort.u.port_v6.data.speed, buffer)));

        json_object_object_add(jsonPort, "linkState",
                json_object_new_string(decodeState(hInfoPort.u.port_v6.data.state, buffer)));

        if (hStatistics.u.query_v2.data.port.aPorts[port].tsType == NT_TIMESTAMP_TYPE_NATIVE_UNIX) {
            //printf("ports timestamp:  %ld\t\t%s\n", hStatistics.u.query_v2.data.port.aPorts[port].ts, decodeTimeStamp(hStatistics.u.query_v2.data.port.aPorts[port].ts, buffer) );
            json_object_object_add(jsonPort, "ts", json_object_new_int64(hStatistics.u.query_v2.data.port.aPorts[port].ts));
            json_object_object_add(jsonPort, "tsStr", json_object_new_string(decodeTimeStamp(hStatistics.u.query_v2.data.port.aPorts[port].ts, buffer)));
        } else {
            json_object_object_add(jsonPort, "ts", json_object_new_int64(hStatistics.u.query_v2.data.port.aPorts[port].ts));
        }
        json_object_object_add(jsonPort, "tsType",
                json_object_new_string(decodeTimeStampType(hStatistics.u.query_v2.data.port.aPorts[port].tsType, buffer)));


        // Print the RMON1 pkts and octets counters
        if ((!opt_txport) && (hStatistics.u.query_v2.data.port.aPorts[port].rx.valid.RMON1)) {
            //            struct json_object *jsonRx = json_object_new_object();
            portSummary2JSON(jsonPort, hStatistics.u.query_v2.data.port.aPorts[port].rx);
            //            json_object_object_add(jsonPort, "Rx", jsonRx);

        } else {
            //printf("Port%d doesn't support RMON1 RX counters.\n", port);
        }
        jsonPorts[port] = jsonPort;
    }

    return port; //jsonPorts;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//
static struct json_object * getStreamData(NtInfoStream_t hInfo, NtStatStream_t hStat) {
    char buffer[NT_ERRBUF_SIZE]; // Error buffer
    int status; // Status variable
    NtStatistics_t stat; // Statistics data
    NtInfo_t hStreamInfo;
    uint32_t stream = 0;

    // Read the info on all streams instantiated in the system
    hStreamInfo.cmd = NT_INFO_CMD_READ_STREAM;
    if ((status = NT_InfoRead(hInfo, &hStreamInfo)) != NT_SUCCESS) {
        NT_ExplainError(status, buffer, sizeof (buffer) - 1);
        fprintf(stderr, "NT_InfoRead() failed: %s\n", buffer);
        return NULL;
    }

    stat.cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
    stat.u.query_v2.poll = 0; // Wait for a new set
    stat.u.query_v2.clear = 0; // Don't clear statistics
    if ((status = NT_StatRead(hStat, &stat)) != NT_SUCCESS) {
        // Get the status code as text
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, "NT_StatRead() failed: %s\n", buffer);
        return NULL;
    }

    // Put the stream count and timestamp into the base object
    //
    struct json_object *jsonStreams = json_object_new_object();
    json_object_object_add(jsonStreams, "numStreams", json_object_new_int((int) hStreamInfo.u.stream.data.count));

    json_object_object_add(jsonStreams, "timestamp", json_object_new_int64(stat.u.query_v2.data.stream.ts));
    json_object_object_add(jsonStreams, "timestampStr", json_object_new_string(decodeTimeStamp(stat.u.query_v2.data.stream.ts, buffer)));

    // Create an array object to hold info/stats for each of the instantiated streams
    //
    struct json_object *jsonStreamsAry = json_object_new_array();
    for (stream = 0; stream < hStreamInfo.u.stream.data.count; ++stream) {
        NtInfo_t hStreamIdInfo;

        uint32_t streamID = hStreamInfo.u.stream.data.streamIDList[stream];

        // Read the info on this specific stream
        hStreamIdInfo.cmd = NT_INFO_CMD_READ_STREAMID;
        hStreamIdInfo.u.streamID.streamId = streamID;
        if ((status = NT_InfoRead(hInfo, &hStreamIdInfo)) != NT_SUCCESS) {
            NT_ExplainError(status, buffer, sizeof (buffer) - 1);
            fprintf(stderr, "NT_InfoRead() failed: %s\n", buffer);
            return NULL;
        }

        // Create a new Stream object to hold the info and stats
        //
        struct json_object *jsonStream = json_object_new_object();
        json_object_object_add(jsonStream, "id", json_object_new_int(streamID));

        // Output the Stream Info to JSON object
        //
        struct json_object *jsonStreamInfo = json_object_new_object();
        json_object_object_add(jsonStreamInfo, "minHostBufferSize", json_object_new_int(hStreamIdInfo.u.streamID.data.minHostBufferSize));
        json_object_object_add(jsonStreamInfo, "numHostBuffers", json_object_new_int(hStreamIdInfo.u.streamID.data.numHostBuffers));
        json_object_object_add(jsonStreamInfo, "numaNode", json_object_new_int(hStreamIdInfo.u.streamID.data.numaNode));
        json_object_object_add(jsonStreamInfo, "useCount", json_object_new_int(hStreamIdInfo.u.streamID.data.useCount));

        switch (hStreamIdInfo.u.streamID.data.state) {
            case NT_STREAM_ID_STATE_UNKNOWN: snprintf(buffer, NT_ERRBUF_SIZE, "UNKNOWN");
                break;
            case NT_STREAM_ID_STATE_ACTIVE: snprintf(buffer, NT_ERRBUF_SIZE, "ACTIVE");
                break;
            case NT_STREAM_ID_STATE_INACTIVE: snprintf(buffer, NT_ERRBUF_SIZE, "INACTIVE");
                break;
            default: snprintf(buffer, NT_ERRBUF_SIZE, "ERROR: not defined");
        }
        json_object_object_add(jsonStreamInfo, "state", json_object_new_string(buffer));

        // Put the info object into the stream object
        json_object_object_add(jsonStream, "info", jsonStreamInfo);

        // Output the Stream Statistics to JSON object
        //
        struct json_object *jsonStreamStat = json_object_new_object();
        json_object_object_add(jsonStreamStat, "pkts_forward", json_object_new_int64((int64_t) stat.u.query_v2.data.stream.streamid[streamID].forward.pkts));
        json_object_object_add(jsonStreamStat, "octets_forward", json_object_new_int64((int64_t) stat.u.query_v2.data.stream.streamid[streamID].forward.octets));
        json_object_object_add(jsonStreamStat, "pkts_drop", json_object_new_int64((int64_t) stat.u.query_v2.data.stream.streamid[streamID].drop.pkts));
        json_object_object_add(jsonStreamStat, "octets_drop", json_object_new_int64((int64_t) stat.u.query_v2.data.stream.streamid[streamID].drop.octets));
        json_object_object_add(jsonStreamStat, "pkts_flush", json_object_new_int64((int64_t) stat.u.query_v2.data.stream.streamid[streamID].flush.pkts));
        json_object_object_add(jsonStreamStat, "octets_flush", json_object_new_int64((int64_t) stat.u.query_v2.data.stream.streamid[streamID].flush.octets));

        // Put the stats object into the stream object
        json_object_object_add(jsonStream, "stats", jsonStreamStat);

        // put the Stream object into the array.
        json_object_array_add(jsonStreamsAry, jsonStream);
    }

    // encapsulate the Streams array into an object.
    json_object_object_add(jsonStreams, "streams", jsonStreamsAry);

    return jsonStreams;
}

static struct json_object * getColorStats(NtInfoStream_t hInfo, NtStatStream_t hStat, uint32_t adapter) {
    char buffer[NT_ERRBUF_SIZE]; // Error buffer
    int status; // Status variable
    NtStatistics_t stat; // Statistics data
    NtInfo_t hStreamInfo;
    uint32_t color = 0;

    // Read the info on all streams instantiated in the system
    hStreamInfo.cmd = NT_INFO_CMD_READ_STREAM;
    if ((status = NT_InfoRead(hInfo, &hStreamInfo)) != NT_SUCCESS) {
        NT_ExplainError(status, buffer, sizeof (buffer) - 1);
        fprintf(stderr, "NT_InfoRead() failed: %s\n", buffer);
        return NULL;
    }

    stat.cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
    stat.u.query_v2.poll = 0; // Wait for a new set
    stat.u.query_v2.clear = 0; // Don't clear statistics
    if ((status = NT_StatRead(hStat, &stat)) != NT_SUCCESS) {
        // Get the status code as text
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, "NT_StatRead() failed: %s\n", buffer);
        return NULL;
    }

    // Put the stream count and timestamp into the base object
    //
    struct json_object *jsonColors = json_object_new_object();

    //printf("stream timestamp: %ld\t\t%s\n", stat.u.query_v2.data.stream.ts, decodeTimeStamp(stat.u.query_v2.data.stream.ts, buffer) );
    json_object_object_add(jsonColors, "timestamp", json_object_new_int64(stat.u.query_v2.data.adapter.aAdapters[adapter].color.ts));
    json_object_object_add(jsonColors, "timestampStr", json_object_new_string(decodeTimeStamp(stat.u.query_v2.data.adapter.aAdapters[adapter].color.ts, buffer)));
    //    printf("\n");

    struct json_object *jsonColorAry = json_object_new_array();
    for (color = 0; color < NUM_COLOR_COUNTERS; ++color) {

        // Output the Color Statistics to JSON object
        //
        if (stat.u.query_v2.data.adapter.aAdapters[adapter].color.aColor[color].pkts) {
            struct json_object *jsonColorStat = json_object_new_object();
            json_object_object_add(jsonColorStat, "id", json_object_new_int64((int64_t) color));
            json_object_object_add(jsonColorStat, "pkts", json_object_new_int64((int64_t) stat.u.query_v2.data.adapter.aAdapters[adapter].color.aColor[color].pkts));
            json_object_object_add(jsonColorStat, "octets", json_object_new_int64((int64_t) stat.u.query_v2.data.adapter.aAdapters[adapter].color.aColor[color].octets));

            // put the Stream object into the array.
            json_object_array_add(jsonColorAry, jsonColorStat);
        }
    }

    // encapsulate the Streams array into an object.
    json_object_object_add(jsonColors, "colors", jsonColorAry);

    return jsonColors;
}

static struct json_object * getMergedColorStats(NtInfoStream_t hInfo, NtStatStream_t hStat, uint32_t numAdapters) {
    char buffer[NT_ERRBUF_SIZE]; // Error buffer
    int status; // Status variable
    NtStatistics_t stat; // Statistics data
    NtInfo_t hStreamInfo;
    uint32_t color = 0;
    uint32_t adapter;
    uint64_t colorStatsPkts[NUM_COLOR_COUNTERS] = {0};
    uint64_t colorStatsOctets[NUM_COLOR_COUNTERS] = {0};

    // Read the info on all streams instantiated in the system
    hStreamInfo.cmd = NT_INFO_CMD_READ_STREAM;
    if ((status = NT_InfoRead(hInfo, &hStreamInfo)) != NT_SUCCESS) {
        NT_ExplainError(status, buffer, sizeof (buffer) - 1);
        fprintf(stderr, "NT_InfoRead() failed: %s\n", buffer);
        return NULL;
    }

    stat.cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
    stat.u.query_v2.poll = 0; // Wait for a new set
    stat.u.query_v2.clear = 0; // Don't clear statistics
    if ((status = NT_StatRead(hStat, &stat)) != NT_SUCCESS) {
        // Get the status code as text
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, "NT_StatRead() failed: %s\n", buffer);
        return NULL;
    }

    // Put the stream count and timestamp into the base object
    //
    struct json_object *jsonColors = json_object_new_object();

    for (adapter = 0; adapter < numAdapters; ++adapter) {
        for (color = 0; color < NUM_COLOR_COUNTERS; ++color) {

            colorStatsPkts[color] += stat.u.query_v2.data.adapter.aAdapters[adapter].color.aColor[color].pkts;
            colorStatsOctets[color] += stat.u.query_v2.data.adapter.aAdapters[adapter].color.aColor[color].octets;
        }
    }

    struct json_object *jsonColorAry = json_object_new_array();
    for (color = 0; color < NUM_COLOR_COUNTERS; ++color) {
        // Output the Color Statistics to JSON object
        //
        if (colorStatsPkts[color]) {
            struct json_object *jsonColorStat = json_object_new_object();
            json_object_object_add(jsonColorStat, "id", json_object_new_int64((int64_t) color));
            json_object_object_add(jsonColorStat, "pkts", json_object_new_int64(colorStatsPkts[color]));
            json_object_object_add(jsonColorStat, "octets", json_object_new_int64(colorStatsOctets[color]));

            // put the Stream object into the array.
            json_object_array_add(jsonColorAry, jsonColorStat);
        }
    }
    json_object_object_add(jsonColors, "colors", jsonColorAry);



    return jsonColors;
}

static int validateFilename(char *filename) {
    unsigned int i = 0;

    if (strnlen(filename, FILENAME_MAXLEN) == FILENAME_MAXLEN) {
        printf("\nERROR: Filename cannot excceed %d characters!\n\n", FILENAME_MAXLEN);
        return 0;
    }
    for (i = 0; i < strnlen(filename, FILENAME_MAXLEN); ++i) {
        if (((filename[i] >= 'a') && (filename[i] <= 'z')) ||
                ((filename[i] >= 'A') && (filename[i] <= 'Z')) ||
                ((filename[i] >= '0') && (filename[i] <= '9')) ||
                (filename[i] == '.') ||
                (filename[i] == '/') ||
                (filename[i] == '_')
                ) {

            continue;
        } else {
            return 0;
        }

    }
    return 1;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
int main(int argc, char *argv[]) {
    int numOptions = 0;
    int option;
    uint i = strnlen(&argv[0][0], 512);
    int status;
    char *p = argv[0];
    int option_index = 0;
    char errorBuffer[NT_ERRBUF_SIZE]; // Error buffer
    NtStatStream_t hStat; // Handle to a statistics stream
    NtInfoStream_t hInfo;
    char buffer[NT_ERRBUF_SIZE]; // Error buffer
    char filename[FILENAME_MAXLEN];
    uint32_t numAdapters;
    uint32_t numPorts;
    uint32_t adapter;
    FILE *json_file;
    uint all_output_options = true;

    signal(SIGINT, StopApplication);

    //-----    -----    -----    -----    -----    -----    -----    -----
    // Process command line options
    // Strip path
    for (; i != 0; i--) {
        if ((p[i - 1] == '.') || (p[i - 1] == '\\') || (p[i - 1] == '/')) {
            break;
        }
    }

    while ((option = getopt_long(argc, argv, "f:pvh", long_options, &option_index)) != -1) {
        numOptions++;
        switch (option) {
            case OPTION_HELP:
            case OPTION_SHORT_HELP:
                _Usage();
                break;
            case OPTION_FILE:
                if (validateFilename(optarg)) {
                    snprintf(filename, FILENAME_MAXLEN, "%s", optarg);
                    opt_filename = true;
                    printf("OPTION_FILE: %s\n", filename);
                } else {
                    printf("Invalid filename.  Filename can only contain alphanumeric characters and \'.\', \'_\' or \'/\' \n");
                    return -1;
                }
                //memcpy(filename, optarg, strlen(optarg));
                break;
            case OPTION_PRETTY:
                opt_pretty_print = true;
                printf("OPTION_PRETTY: %d\n", opt_pretty_print);
                break;
            case OPTION_VERBOSE:
                opt_verbose = true;
                printf("OPTION_VERBOSE: %d\n", opt_verbose);
                break;

            case OPTION_SYSTEM:
                opt_system = true;
                all_output_options = false;
                break;

            case OPTION_TIMESYNC:
                opt_timesync = true;
                all_output_options = false;
                break;

            case OPTION_SENSORS:
                if (opt_alarm) {
                    printf("\nERROR: alarm and sensors option cannot both be specified!\n\n");
                    exit(-1);
                }
                opt_sensors = true;
                all_output_options = false;
                break;

            case OPTION_ALARM:
                if (opt_sensors) {
                    printf("\nERROR: alarm and sensors option cannot both be specified!\n\n");
                    exit(-1);
                }
                opt_alarm = true;
                all_output_options = false;
                break;

            case OPTION_PORT:
                opt_port = true;
                all_output_options = false;
                break;

            case OPTION_RXPORT:
                opt_rxport = true;
                all_output_options = false;
                break;

            case OPTION_TXPORT:
                opt_txport = true;
                all_output_options = false;
                break;

            case OPTION_PORTSIMPLE:
                opt_portsummary = true;
                all_output_options = false;
                break;


            case OPTION_STREAM:
                opt_stream = true;
                all_output_options = false;
                break;

            case OPTION_COLOR:
                opt_color = true;
                all_output_options = false;
                break;

            case OPTION_MERGEDCOLOR:
                opt_mergedcolor = true;
                all_output_options = false;
                break;


            case OPTION_PRODUCT:
                opt_product = true;
                all_output_options = false;
                break;

            default:
                printf("ERROR: Invalid command line option!!!\n");
                return -1;
                break;
        }
    }

    if (all_output_options) {
        opt_system = true;
        opt_timesync = true;
        opt_sensors = true;
        opt_port = true;
        opt_rxport = true;
        opt_txport = true;
        opt_stream = true;
        opt_color = true;
        opt_product = true;
    }

    if (opt_filename) {
        printf("\nWriting output to file: %s\n", filename);
    } else {
        printf("\nNo file specified - writing to stdout.\n");
        opt_verbose = true;
    }

    printf("\nOutputing the following objects:\n");
    if (opt_system) printf("    system\n");
    if (opt_product) printf("    product\n");
    if (opt_timesync) printf("    timesync\n");
    if (opt_sensors) printf("    sensors\n");
    if (opt_alarm) printf("    alarm\n");
    if (opt_port) printf("    port\n");
    if (opt_rxport) printf("    rxport\n");
    if (opt_txport) printf("    txport\n");
    if (opt_portsummary) printf("    opt_portsummary\n");
    if (opt_stream) printf("    stream\n");
    if (opt_color) printf("    color\n");
    printf("\n");

    //-----    -----    -----    -----    -----    -----    -----    -----
    // Initialize the NTAPI library
    if ((status = NT_Init(NTAPI_VERSION)) != NT_SUCCESS) {
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
        fprintf(stderr, "NT_Init() failed: %s\n", errorBuffer);
        return status;
    }

    // Open the info and Statistics
    if ((status = NT_InfoOpen(&hInfo, "InfoStream")) != NT_SUCCESS) {
        NT_ExplainError(status, buffer, sizeof (buffer) - 1);
        fprintf(stderr, "NT_InfoOpen() failed: %s\n", buffer);
        return -1;
    }

    if ((status = NT_StatOpen(&hStat, "StatsStream")) != NT_SUCCESS) {
        // Get the status code as text
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, "NT_StatOpen() failed: %s\n", buffer);
        return -1;
    }

    //-----    -----    -----    -----    -----    -----    -----    -----
    struct json_object *jsonContainer = json_object_new_object();
    struct json_object *jsonAdapters = json_object_new_array();
    struct json_object *jsonPorts = json_object_new_object();


    struct json_object *systemData = getSystemInfo(hInfo, &numAdapters, &numPorts);
    struct json_object *jsonSummaryPorts[numPorts];

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t sysTime = ((tv.tv_sec * 1000000) + tv.tv_usec)*100;

    json_object_object_add(jsonContainer, "sysTime", json_object_new_int64(sysTime));
    json_object_object_add(jsonContainer, "sysTimeStr", json_object_new_string(decodeTimeStamp(sysTime, errorBuffer)));

    struct json_object *jsonMergedColorStats = getMergedColorStats(hInfo, hStat, numAdapters);

    for (adapter = 0; adapter < numAdapters; adapter++) {
        uint32_t numSensors = 0;
        uint32_t firstPort = 0;
        uint32_t lastPort = 0;

        struct json_object *jsonAdapter = getAdapterInfo(hInfo, adapter, &numSensors, &firstPort, &lastPort);
        struct json_object *jsonSensors = getSensorInfo(adapter, hInfo, numSensors);
        struct json_object *jsonPortSensors = getPortSensorInfo(hInfo, firstPort, lastPort);

        struct json_object *jsonColorStats = getColorStats(hInfo, hStat, adapter);

        struct json_object *jsonTimeSyncInfo = getTimeSyncInfo(hInfo, adapter);
        struct json_object *jsonTimeSyncStatExt = getTimeSyncStatExt(hInfo, adapter);
        struct json_object *jsonTimeSyncStat = getTimeSyncStat(hInfo, adapter);
        struct json_object *jsonTimeSyncContainer = json_object_new_object();

        json_object_object_add(jsonTimeSyncContainer, "Info", jsonTimeSyncInfo);
        json_object_object_add(jsonTimeSyncContainer, "ExternalStats", jsonTimeSyncStatExt);
        json_object_object_add(jsonTimeSyncContainer, "Stats", jsonTimeSyncStat);

        if (opt_timesync) {
            json_object_object_add(jsonAdapter, "TimeSync", jsonTimeSyncContainer);
        }

        if (opt_sensors || opt_alarm) {
            json_object_array_add(jsonSensors, jsonPortSensors);
            json_object_object_add(jsonAdapter, "sensors", jsonSensors);
        }

        if (opt_color) {
            json_object_object_add(jsonAdapter, "colorStats", jsonColorStats);
        }

        json_object_array_add(jsonAdapters, jsonAdapter);
    }

    if (!opt_portsummary) {

        struct json_object *jsonPortInfo = getPortInfo(hInfo);
        struct json_object *jsonPortStats = getPortStats(hInfo, hStat);
        struct json_object *jsonStreamData = getStreamData(hInfo, hStat);

        json_object_object_add(jsonPorts, "info", jsonPortInfo);
        json_object_object_add(jsonPorts, "stats", jsonPortStats);

        if (opt_system) {
            json_object_object_add(jsonContainer, "system", systemData);
        }

        if (opt_timesync || opt_product || opt_sensors || opt_alarm || opt_color) {
            json_object_object_add(jsonContainer, "adapters", jsonAdapters);
        }
        if (opt_port || opt_rxport || opt_txport) {
            json_object_object_add(jsonContainer, "portData", jsonPorts);
        }

        if (opt_stream) {
            json_object_object_add(jsonContainer, "streamData", jsonStreamData);
        }

        if (opt_mergedcolor) {
            json_object_object_add(jsonContainer, "mergedColor", jsonMergedColorStats);
        }
    } else {
        numPorts = getPortSummary(jsonSummaryPorts, sysTime, hInfo, hStat);
    }



    //-----    -----    -----    -----    -----    -----    -----    -----
    if (opt_filename) {
        //printf("---opening file: %s\n", filename);
        json_file = fopen(filename, "w");
        if (json_file == NULL) {
            fprintf(stderr, "Failed to open %s\n", filename);
            return -1;
        }

        if (!opt_portsummary) {
            if (opt_pretty_print) {
                fprintf(json_file, "%s", json_object_to_json_string_ext(jsonContainer, JSON_C_TO_STRING_PRETTY));
            } else {
                fprintf(json_file, "%s", json_object_to_json_string(jsonContainer));
            }
        } else {
            uint32_t port;
            for (port = 0; port < numPorts; ++port) {
                if (opt_pretty_print) {
                    fprintf(json_file, "%s\n", json_object_to_json_string_ext(jsonSummaryPorts[port], JSON_C_TO_STRING_PRETTY));
                } else {
                    fprintf(json_file, "%s", json_object_to_json_string(jsonSummaryPorts[port]));
                }
            }
        }
    } else {
        if (!opt_portsummary) {
            printf("\n----- Start of JSON ----->>>\n%s\n<<<----- End of JSON -----\n\n",
                    json_object_to_json_string_ext(jsonContainer, JSON_C_TO_STRING_PRETTY));

        } else {
            printf("\n----- Start of JSON ----->>>\n");
            uint32_t port;
            for (port = 0; port < numPorts; ++port) {
                printf("%s\n",
                        json_object_to_json_string_ext(jsonSummaryPorts[port], JSON_C_TO_STRING_PRETTY));
            }
            printf("\n<<<----- End of JSON -----\n\n");
        }
    }

    // CLEAN UP NT Resources
    // Close the info stream
    if ((status = NT_InfoClose(hInfo)) != NT_SUCCESS) {
        NT_ExplainError(status, buffer, sizeof (buffer) - 1);
        fprintf(stderr, "NT_InfoClose() failed: %s\n", buffer);
        return -1;
    }

    // Close the statistics stream
    if ((status = NT_StatClose(hStat)) != NT_SUCCESS) {
        // Get the status code as text
        NT_ExplainError(status, buffer, sizeof (buffer));
        fprintf(stderr, "NT_StatClose() failed: %s\n", buffer);
        return -1;
    }

    NT_Done();

    return 0;
}


