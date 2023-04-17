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


/**
 * @internal
 * This source file contains <b>closed source code</b> to replay data from disk.
 */

#ifdef WIN32
#include "platformwin.h"
#else
#define _GNU_SOURCE
#endif
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "nt.h"

#if defined(__linux__) || defined(__FreeBSD__)
  #include <signal.h>
  #include <pthread.h>
  #include <unistd.h>
#endif

#include <ctype.h>
#include <assert.h>

#include "nttime.h"
#include "shared/argparse/argparse.h"
#include "queue.h"

#include "tool_def.h"
#include "boundary_replay.h"
#include "tool_common.h"


/******************************************************************************/
/* Internal variables                                                         */
/******************************************************************************/
static NtApplExitCode_t ApplExitCode = NT_APPL_SUCCESS;

static volatile int appRunning = 1;
static int threadReady = 0;
static int txWait = 1;
// Complete version string constant which can be searched for in the binary
static const char* progname = "replay (v. " TOOLS_RELEASE_VERSION ")";
static unsigned long long bytes = 0;
static unsigned long long totBytes = 0;
static unsigned long long segments = 0;

static char* opt_file = NULL;
static uint32_t opt_port = -1;
static uint32_t opt_loop = 0;
static uint32_t opt_path_delay = 0;

static uint64_t adapterTimeOffset = 0;
static uint64_t lastPacketTS = 0;
static uint64_t firstPacketTS = 0;
static uint64_t absoluteStartTime = 0;
static uint32_t modFactor = -1;
static NtNetFileType_e fileType;
static NtNetFileReadDesc_t descriptorType;
static uint32_t boundary_time = 1000000;  // 10 mSec default

static int adapterNo = -1;
static int numPorts = -1;
static uint64_t portMask = 0;
static int featureLevel = -1;

static enum NtProfileType_e profile;
static enum NtTimestampType_e timestampType;
static enum NtTxTimingMethod_e txTiming;

static bool is4GA = false;
static bool zeroCopyTx = false;
static bool zeroCopyTx4GA = false;
static bool timedTx = false;
static bool timedTx4GA = false;

static char* opt_start_time = NULL;

/**
 * Table of valid options.
 */
struct argparse_option arg_options[] = {
  OPT_HELP(),
  OPT_STRING( 'f', "file",       &opt_file,       "Specifies the file to replay.", NULL, 0, 0, "file name"),
  OPT_INTEGER('p', "port",       &opt_port,       "Port on which a pcap file should be replayed", NULL, 0, 0, "opt_port"),
  OPT_INTEGER('l', "loop",       &opt_loop,       "limit replay this many iterations of the file and terminate", NULL, 0, 0, "opt_loop"),
  OPT_INTEGER('m', "modfac",     &modFactor,      "Transmission begin time stamp modulus factor in seconds (default 15 secs.).", NULL, 0, 0, "modfac"),
  OPT_INTEGER('d', "path_delay", &opt_path_delay, "Path Delay in 10 nSec clicks", NULL, 0, 0, "opt_path_delay"),
  OPT_STRING( 's', "starttime",  &opt_start_time, "Absolute transmission begin time stamp (UTC).\n"
                                                  "Format: YYYY/MM/DD-HH:MM:SS, HH:MM:SS or HH:MM", NULL, 0, 0, "starttime"),
  OPT_END(),
};

//
//
//
static void DisplayProgramHeader(void)
{
  printf("\r%s\n", progname);

  /* Print separator line */
  for (int i = 0; i < 78; ++i) {
    printf("=");
  }
  printf("\n");
}

//
//
//
static void DisplayProgramFooter (void)
{
  printf("\n");
}

/*
 * The function called when user is pressing CTRL-C
 */
static HANDLER_API StopApplication(int sig NOUSE)
{
  appRunning = 0;
  HANDLER_RETURN_OK
}

//
//
//
static void read4GACapabilities(NtInfoStream_t hInfo, const int adapterNo)
{
  NtInfo_t infoMsg;

  // Get arch generation
  infoMsg.cmd = NT_INFO_CMD_READ_PROPERTY;
  snprintf(infoMsg.u.property.path, sizeof(infoMsg.u.property.path), "Adapter%d.FpgaGeneration", adapterNo);
  if (NT_InfoRead(hInfo, &infoMsg) == NT_SUCCESS) {
    is4GA = infoMsg.u.property.data.u.i >= 4;
  }

  // NT4GA adapters with "real" TBH based Tx does not support NT_NET_SET_PKT_DESCR_TYPE_EXT7 descriptor type
  infoMsg.cmd = NT_INFO_CMD_READ_PROPERTY;
  snprintf(infoMsg.u.property.path, sizeof(infoMsg.u.property.path), "Adapter%d.tx.ZeroCopyTransmit", adapterNo);
  if (NT_InfoRead(hInfo, &infoMsg) == NT_SUCCESS) {
    zeroCopyTx = infoMsg.u.property.data.u.i != 0;
  } else {
    zeroCopyTx = 0;
  }
  zeroCopyTx4GA = (is4GA && zeroCopyTx);

  infoMsg.cmd = NT_INFO_CMD_READ_PROPERTY;
  snprintf(infoMsg.u.property.path, sizeof(infoMsg.u.property.path), "Adapter%d.tx.TimedTransmit", adapterNo);
  if (NT_InfoRead(hInfo, &infoMsg) == NT_SUCCESS) {
    timedTx = infoMsg.u.property.data.u.i != 0;
  } else {
    timedTx = 0;
  }
  timedTx4GA = (is4GA && timedTx);
}

//
//
//
static uint32_t CheckCapabilities(const int portNo, bool* checksPassed)
{
  int status;                     // Status variable
  NtInfoStream_t hInfo;           // Handle to an info stream
  NtInfo_t infoRead;

  *checksPassed = false;

  if ((status = NT_InfoOpen(&hInfo, "replay")) != NT_SUCCESS) {
    return status;
  }

  infoRead.cmd = NT_INFO_CMD_READ_PORT_V9;
  infoRead.u.port_v9.portNo = (uint8_t)(portNo);

  if ((status = NT_InfoRead(hInfo, &infoRead)) != NT_SUCCESS) {
    NT_InfoClose(hInfo);
    return status;
  }

  numPorts = infoRead.u.port_v9.data.adapterInfo.numPorts;
  adapterNo = infoRead.u.port_v9.data.adapterNo;
  txTiming = infoRead.u.port_v9.data.adapterInfo.txTiming;
  timestampType = infoRead.u.port_v9.data.adapterInfo.timestampType;
  featureLevel = infoRead.u.port_v9.data.adapterInfo.featureLevel;
  profile = infoRead.u.port_v9.data.adapterInfo.profile;

  if (portNo != -1) {
	  portMask = (uint64_t)1<<portNo;
  }

  read4GACapabilities(hInfo, adapterNo);

  printf("The ports are located on a %s profile.\n",
         profile == NT_PROFILE_TYPE_CAPTURE?"Capture":
         profile == NT_PROFILE_TYPE_CAPTURE_REPLAY?"Capture/Replay":
         profile == NT_PROFILE_TYPE_TRAFFIC_GEN?"Traffic gen":
         profile == NT_PROFILE_TYPE_INLINE?"Inline":"Unknown");

  if ((profile != NT_PROFILE_TYPE_CAPTURE_REPLAY) && !zeroCopyTx4GA) {
    fprintf(stderr, "This tool doesn't support this profile. Please use the \"CaptureReplay\" profile.\n");
    return NT_SUCCESS;
  }

    infoRead.cmd = NT_INFO_CMD_READ_PORT_V9;
    infoRead.u.port_v9.portNo = portNo;
    if ((status = NT_InfoRead(hInfo, &infoRead)) != NT_SUCCESS) {
      NT_InfoClose(hInfo);
      return status;
    }

    if (infoRead.u.port_v9.data.capabilities.featureMask&NT_PORT_FEATURE_RX_ONLY) {
      fprintf(stderr, "Transmit is not possible on this port on the selected adapter: Port %d\n", portNo);
      NT_InfoClose(hInfo);
      return NT_SUCCESS;
    }

  NT_InfoClose(hInfo);
  *checksPassed = true;
  return NT_SUCCESS;
}


static int readInfo(NtNetStreamFile_t hNetFile, enum NtNetFileReadCmd_e cmd, NtNetFileRead_t* data)
{
  int status;
  char errorBuffer[NT_ERRBUF_SIZE];

  data->cmd = cmd;
  if ((status = NT_NetFileRead(hNetFile, data)) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NtNetFileRead_t() failed: %s\n", errorBuffer);
    return -1;
  }

  return 0;
}

//
//
//
static int GetFileData(void)
{
  int status;                     // Status variable
  NtNetStreamFile_t hNetFile;       // Handle to the File stream
  NtNetBuf_t hNetBufFile;           // Net buffer container. Used to return packets from the file stream
  NtNetFileRead_t data;
  char errorBuffer[NT_ERRBUF_SIZE];

  // Open the capture file to to read the type of data)
  if ((status = NT_NetFileOpen_v2(&hNetFile, "replay", NT_NET_INTERFACE_SEGMENT, opt_file, timedTx4GA)) != NT_SUCCESS) {
    // Get the status code as text
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_NetFileOpen() failed: %s\n", errorBuffer);
    return 1;
  }

  // Dummy get so we can retreive the first packet timestamp
  if ((status = NT_NetFileGet(hNetFile, &hNetBufFile)) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NtNetFileRead_t() failed: %s\n", errorBuffer);
    return 1;
  }

  if (readInfo(hNetFile, NT_NETFILE_READ_INFO_CMD_V1, &data) == -1) {
    return 1;
  }
  firstPacketTS = data.u.info_v1.firstTimestamp;

  if (readInfo(hNetFile, NT_NETFILE_READ_FILETYPE_CMD, &data) == -1) {
    return 1;
  }
  fileType = data.u.fileType;

  if (readInfo(hNetFile, NT_NETFILE_READ_DESCRIPTOR_CMD, &data) == -1) {
    return 1;
  }
  descriptorType = data.u.desc;
  if (descriptorType.tsType == NT_TIMESTAMP_TYPE_UNIX_NANOTIME) {
    /* Convert to 10ns format */
    firstPacketTS /= 10;
  }

  uint64_t max = (uint64_t)boundary_time;

  while(appRunning) {
  /*  Get a packet buffer pointer from the segment. */
	struct NtNetBuf_s pktNetBuf;
	_nt_net_build_pkt_netbuf(hNetBufFile, &pktNetBuf);

	uint32_t pkt = 0;
	do {
	  // Get the timestamp from the packet as stored in the file and add 10 mSec * the iteration
	  uint64_t timestamp =  NT_NET_GET_PKT_TIMESTAMP((&pktNetBuf));

	  uint64_t diff = timestamp - firstPacketTS;

//	  timestamp -= opt_path_delay;
//	  printf("%d - ts: %ld   diff: %ld\n", pkt++, timestamp, diff);

	  if ( diff > max ) {
		  printf("file too long!  max_time: %ld\n", max);
		  return 2;
	  }
	} while (_nt_net_get_next_packet(hNetBufFile,
									 NT_NET_GET_SEGMENT_LENGTH(hNetBufFile),
									 &pktNetBuf) > 0);

	  // Dummy get so we can retreive the first packet timestamp
	  if ((status = NT_NetFileGet(hNetFile, &hNetBufFile)) != NT_SUCCESS) {
			if (status == NT_STATUS_END_OF_FILE) {
				  break;
			} else {
				NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
				fprintf(stderr, "NtNetFileRead_t() failed: %s\n", errorBuffer);
				return 1;
			}
	  }
  }

  // Close the file stream
  NT_NetFileClose(hNetFile);
  return 0;
}

//
//
//
static uint64_t CalcStartTS(uint64_t ts, uint32_t mod_fac)
{
  uint64_t tmp_ts;

  /* Round adapter time up to nearest second */
  tmp_ts = (ts + 100000000U - 1) / 100000000U;
  /* Add one additional second to ensure ample time for set-up */
  tmp_ts += 1U;
  /* Find nearest time that satisfies tmp_ts % mod_fac == 0 */
  tmp_ts = ((tmp_ts + (mod_fac - 1)) / mod_fac) * mod_fac;

  return tmp_ts * 100000000U; /* convert secs -> 10-ns ticks */
}

/**
 * @brief Gets the timestamp from the adapter
 *
 * This function is called to get the adapter timestamp
 *
 * @param[in]    adapterNo   Adapter number to get timestamp from
 * @param[out]   ts          Pointer to the timestamp
 *
 * @retval  NT_SUCCESS       Function succeded
 * @retval  != NT_SUCCESS    Function failed
 */
static uint32_t GetAdapterTimestamp(int adapterNo, uint64_t *ts)
{
  uint32_t status;
  char errorBuffer[NT_ERRBUF_SIZE];
  NtConfigStream_t hStream;
  NtConfig_t configRead;

  if ((status = NT_ConfigOpen(&hStream, "Replay")) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_ConfigOpen() failed: %s\n", errorBuffer);
    NT_ConfigClose(hStream);
    return status;
  }

  configRead.parm = NT_CONFIG_PARM_ADAPTER_TIMESTAMP;
  configRead.u.timestampRead.adapter = (uint8_t)adapterNo;
  if ((status = NT_ConfigRead(hStream, &configRead)) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_ConfigRead() failed: %s\n", errorBuffer);
    NT_ConfigClose(hStream);
    return status;
  }
  *ts = configRead.u.timestampRead.data.nativeUnixTs;

  if ((status = NT_ConfigClose(hStream)) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_ConfigOpen() failed: %s\n", errorBuffer);
    return status;
  }
  return NT_SUCCESS;
}

//
//
//
static uint32_t CalculateOffset(int adapterNo)
{
  int status;
  uint64_t adapterTS = 0;
  // We are runnig global sync and all the prerequisite are fullfilled.
  // Calculate the gsync delay an dwrite it to the adapter.
  if ((status = GetAdapterTimestamp(adapterNo, &adapterTS)) != NT_SUCCESS) {
    fprintf(stderr, "Reading adapter timestamp failed\n");
    return status;
  }

  uint64_t beginTS = 0;
  if (opt_start_time != NULL) {
    /* Validate start time is not in the past */
    if (absoluteStartTime <= adapterTS) {
      fprintf(stderr, "*** Error: Start time is in the past.\n");
      printf("   startTime: %ld (0x%ld)\n   adapterTS: %ld (0x%ld)\n", absoluteStartTime, absoluteStartTime, adapterTS, adapterTS);
//      return 1;
    }
    beginTS = absoluteStartTime;
  }
  else {
    beginTS = CalcStartTS(adapterTS, modFactor);
  }
  adapterTimeOffset = beginTS - firstPacketTS;
  return NT_SUCCESS;
}


//
//
//
static uint32_t AwaitPacketTransmission(int adapterNo)
{
  int status;
  uint64_t ts;
  while (appRunning) {
    // We need to wait for the packets to be transmitted before closing the stream and
    // disabling Global Sync again.
    if ((status = GetAdapterTimestamp(adapterNo, &ts)) != NT_SUCCESS) {
      fprintf(stderr, "Reading adapter timestamp failed\n");
      return status;
    }
    if (ts > (lastPacketTS + adapterTimeOffset)) {
      break;
    }
    tools_sleep(1);
  }

  return NT_SUCCESS;
}

//
//
//
static uint32_t SetPortTransmitOnTimestamp(int portNo, uint64_t timeDelta)
{
  uint32_t status;
  char errorBuffer[NT_ERRBUF_SIZE];       // Error buffer
  NtConfigStream_t hStream;   // Config stream handle
  NtConfig_t configWrite;     // Config stream data container

  // Open the config stream
  if ((status = NT_ConfigOpen(&hStream, "Replay")) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_ConfigOpen() failed: %s\n", errorBuffer);
    NT_ConfigClose(hStream);
    return status;
  }

  configWrite.parm = NT_CONFIG_PARM_PORT_TRANSMIT_ON_TIMESTAMP;
  configWrite.u.transmitOnTimestamp.portNo = (uint8_t) portNo;
  configWrite.u.transmitOnTimestamp.data.timeDelta = timeDelta * 10;
  configWrite.u.transmitOnTimestamp.data.timeDeltaAdjust = 0;
  configWrite.u.transmitOnTimestamp.data.enable = true;
  configWrite.u.transmitOnTimestamp.data.forceTxOnTs = true;

  if ((status = NT_ConfigWrite(hStream, &configWrite)) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_ConfigWrite() failed: %s\n", errorBuffer);
    NT_ConfigClose(hStream);
    return status;
  }

  // Close the config stream
  if ((status = NT_ConfigClose(hStream)) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_ConfigOpen() failed: %s\n", errorBuffer);
    return status;
  }
  return NT_SUCCESS;
}

//
//
//
static uint32_t ClearPortTransmitOnTimestamp(int portNo)
{
  uint32_t status;
  char errorBuffer[NT_ERRBUF_SIZE];       // Error buffer
  NtConfigStream_t hStream;   // Config stream handle
  NtConfig_t configWrite;     // Config stream data container

  // Open the config stream
  if ((status = NT_ConfigOpen(&hStream, "Replay")) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_ConfigOpen() failed: %s\n", errorBuffer);
    NT_ConfigClose(hStream);
    return status;
  }

  configWrite.parm = NT_CONFIG_PARM_PORT_TRANSMIT_ON_TIMESTAMP;
  configWrite.u.transmitOnTimestamp.portNo = (uint8_t) portNo;
  configWrite.u.transmitOnTimestamp.data.timeDelta = 0;
  configWrite.u.transmitOnTimestamp.data.timeDeltaAdjust = 0;
  configWrite.u.transmitOnTimestamp.data.enable = false;
  configWrite.u.transmitOnTimestamp.data.forceTxOnTs = false;

  if ((status = NT_ConfigWrite(hStream, &configWrite)) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_ConfigWrite() failed: %s\n", errorBuffer);
    NT_ConfigClose(hStream);
    return status;
  }

  // Close the config stream
  if ((status = NT_ConfigClose(hStream)) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_ConfigOpen() failed: %s\n", errorBuffer);
    return status;
  }
  return NT_SUCCESS;
}

//
//
//
static uint32_t Setup4GATransmitOnTimestamp(int adapterNo)
{
  int status;
  if ((status = CalculateOffset(adapterNo)) != NT_SUCCESS) {
    fprintf(stderr, "Unable to calculate offset\n");
    return status;
  }

//  for (i = 0; i < numPorts; i++) {
   if ((status=SetPortTransmitOnTimestamp(opt_port, adapterTimeOffset)) != NT_SUCCESS) {
       fprintf(stderr, "Unable to configure transmit on timestamp: Unable to set port %d time delta.\n", opt_port);
       return status;
    }
//  }

  return NT_SUCCESS;
}

//
//
//
static uint32_t Clear4GATransmitOnTimestamp(int adapterNo)
{
  (void)adapterNo;

  int status;
//  for (i = 0; i < numPorts; i++) {
   if ((status=ClearPortTransmitOnTimestamp(opt_port)) != NT_SUCCESS) {
       fprintf(stderr, "Unable to configure transmit on timestamp: Unable to clear port %d time delta.\n", opt_port);
       return status;
    }
//  }

  return NT_SUCCESS;
}


//
// PThread handler function declaration
//
static THREAD_API _ReplayData(void* arg)
{
  (void)arg;

  int status;                     // Status variable
  char errorBuffer[NT_ERRBUF_SIZE];           // Error buffer

  NtNetStreamFile_t hNetFile;       // Handle to the File stream
  NtNetBuf_t hNetBufFile;           // Net buffer container. Used to return packets from the file stream
  NtNetStreamTx_t hNetTx;           // Handle to the TX stream
  NtNetBuf_t hNetBufTx;             // Net buffer container. Used when getting a transmit buffer
  NtNetFileRead_t data;


#define MAX_NUM_TXPORTS (32)        // Max number of txport in packet descriptor (2^5)

  // Open a TX hostbuffer from TX host buffer pool on the NUMA node of the adapter with the specified TX-port
  NtNetTxAttr_t txAttr;
  NT_NetTxOpenAttrInit(&txAttr);
  NT_NetTxOpenAttrSetName(&txAttr, "replay_tx");
  NT_NetTxOpenAttrSetPortMask(&txAttr, portMask);
  NT_NetTxOpenAttrSetNUMA(&txAttr, NT_NETTX_NUMA_ADAPTER_HB);
  NT_NetTxOpenAttrSetMinHostbufferSize(&txAttr, 0);
  NT_NetTxOpenAttrSetDescriptor(&txAttr, descriptorType.desc);
  NT_NetTxOpenAttrSetTimestampType(&txAttr, descriptorType.tsType);

  status = NT_NetTxOpen_Attr(&hNetTx, &txAttr);
  if (status != NT_SUCCESS) {
    appRunning = 0;
    // Get the status code as text
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_NetTxOpen() failed: %s\n", errorBuffer);
    ApplExitCode = NT_APPL_ERROR_NTAPI;
    THREAD_RETURN_OK
  }

  // Replay cannot use PCAP files without timed tx support. Check whether or not the file is a PCAP file
 if (!timedTx4GA && fileType != NT_NETFILE_TYPE_NT) {
    appRunning = 0;
    fprintf(stderr, "\nERROR: This capture file type is not supported by Replay.\n\n");
    ApplExitCode = NT_APPL_ERROR_NTAPI;
    THREAD_RETURN_OK
  }

  // Activate transmit on timestamp. We do this post-cache to avoid taking too long after calculating the delta.
  if (timedTx4GA) {
    // We are running on the new 4GA Transmit on Timestamp functionality.
    status = Setup4GATransmitOnTimestamp(adapterNo);
    if (status != NT_SUCCESS) {
      appRunning = 0;
      fprintf(stderr, "Setup of transmit on timestamp failed\n");
      ApplExitCode = NT_APPL_ERROR_NTAPI;
      THREAD_RETURN_OK
    }
  }

  uint34_t iteration = 0;
  do {
	  // Open the capture file to replay (captured with the capture example)
	  if ((status = NT_NetFileOpen_v2(&hNetFile, "replay", NT_NET_INTERFACE_SEGMENT, opt_file, timedTx4GA)) != NT_SUCCESS) {
		// Get the status code as text
		appRunning = 0;
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_NetFileOpen() failed: %s\n", errorBuffer);
		ApplExitCode = NT_APPL_ERROR_NTAPI;
		THREAD_RETURN_OK
	  }

	  totBytes=0;
	  threadReady = 1;

	  // Get packet segements from the file and transmit them to port 0
		struct NtNetBuf_s pktNetBuf;
		while (appRunning) {
		  // Get a segment
		  if ((status = NT_NetFileGet(hNetFile, &hNetBufFile)) != NT_SUCCESS) {
			if (status == NT_STATUS_END_OF_FILE) {
			  // The file has no more data - read timestamp of last packet
			  if (readInfo(hNetFile, NT_NETFILE_READ_INFO_CMD_V1, &data) == -1) {
				ApplExitCode = NT_APPL_ERROR_NTAPI;
				THREAD_RETURN_OK
			  }
			  lastPacketTS = data.u.info_v1.lastTimestamp;
			  break;
			}
			// Get the status code as text
			appRunning = 0;
			NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
			fprintf(stderr, "NT_NetFileGet() failed: %s\n", errorBuffer);
			ApplExitCode = NT_APPL_ERROR_NTAPI;
			THREAD_RETURN_OK
		  }

		  // Get a TX buffer for this packet
		  do {
			status = NT_NetTxGet(hNetTx, &hNetBufTx, opt_port,
								 NT_NET_GET_SEGMENT_LENGTH(hNetBufFile),
								 NT_NETTX_SEGMENT_OPTION_RAW,
								 1000 /* wait 1s */);
		  } while (status == NT_STATUS_TIMEOUT && appRunning);

		  if (status == NT_STATUS_TIMEOUT)
			break; /* while (appRunning) */

		  if (status != NT_SUCCESS) {
			// Get the status code as text
			appRunning = 0;
			NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
			fprintf(stderr, "NT_NetTxGet() failed: %s\n", errorBuffer);
			ApplExitCode = NT_APPL_ERROR_NTAPI;
			THREAD_RETURN_OK
		  }
		  // Copy the packet into the TX buffer
		  memcpy(NT_NET_GET_SEGMENT_PTR(hNetBufTx),
				 NT_NET_GET_SEGMENT_PTR(hNetBufFile),
				 NT_NET_GET_SEGMENT_LENGTH(hNetBufFile));

			/*  Get a packet buffer pointer from the segment. */
			_nt_net_build_pkt_netbuf(hNetBufTx, &pktNetBuf);

			do {
			  // Get the timestamp from the packet as stored in the file and add 10 mSec * the iteration
			  uint64_t timestamp =  NT_NET_GET_PKT_TIMESTAMP((&pktNetBuf));
			  timestamp -= opt_path_delay;

			  timestamp += iteration * boundary_time;
//			  printf("%d - timestamp: %ld\n", iteration, timestamp);
			  NT_NET_SET_PKT_TIMESTAMP((&pktNetBuf), timestamp);
			} while (_nt_net_get_next_packet(hNetBufTx,
											 NT_NET_GET_SEGMENT_LENGTH(hNetBufTx),
											 &pktNetBuf) > 0);

		  segments++;
		  bytes+=NT_NET_GET_SEGMENT_LENGTH(hNetBufFile);
		  totBytes+=NT_NET_GET_SEGMENT_LENGTH(hNetBufFile);

		  // Release the TX buffer and the packet will be transmitted
		  if ((status = NT_NetTxRelease(hNetTx, hNetBufTx)) != NT_SUCCESS) {
			// Get the status code as text
			appRunning = 0;
			NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
			fprintf(stderr, "NT_NetTxRelease() failed: %s\n", errorBuffer);
			ApplExitCode = NT_APPL_ERROR_NTAPI;
			THREAD_RETURN_OK
		  }

		  // Release the file packet
		  if ((status = NT_NetFileRelease(hNetFile, hNetBufFile)) != NT_SUCCESS) {
			// Get the status code as text
			appRunning = 0;
			NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
			fprintf(stderr, "NT_NetFileRelease() failed: %s\n", errorBuffer);
			ApplExitCode = NT_APPL_ERROR_NTAPI;
			THREAD_RETURN_OK
		  }
		}

	  // Close the file stream
	  NT_NetFileClose(hNetFile);

	  ++iteration;
  } while (appRunning && ((opt_loop == 0) ? 1 : (iteration < opt_loop)) );


  if (timedTx4GA) {
    if (AwaitPacketTransmission(adapterNo) != NT_SUCCESS) {
      NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
      fprintf(stderr, "Waiting for transmission to end failed: %s\n", errorBuffer);
    }

    if ((status = Clear4GATransmitOnTimestamp(adapterNo)) != NT_SUCCESS) {
      appRunning = 0;
      fprintf(stderr, "Disabling transmit on timestamp failed\n");
      ApplExitCode = NT_APPL_ERROR_NTAPI;
      THREAD_RETURN_OK
    }
    NT_NetTxClose(hNetTx);
  } else {
    // Close the TX stream
    NT_NetTxClose(hNetTx);
  }

  appRunning = 0;
  THREAD_RETURN_OK
}


//
//
//
static int ParseStartTime(const char* dts, uint64_t* ts)
{
  char *rem_d;
  struct tm tm;
  time_t t;
  struct tm *loc_time;
  time_t now;

  /* Reset tm structure because strptime is not required to initialize
     all members variables. */
  memset(&tm, 0, sizeof(tm));
  /* Did user specify date+time in ISO format? */
  rem_d = strptime(dts, "%Y/%m/%d-%H:%M:%S", &tm);
  if (!rem_d) {
    /* Did user specify date and time in locale's representation? */
    rem_d = strptime(dts, "%x %X", &tm);
    if (!rem_d) {
      memset(&tm, 0, sizeof(tm));
      /* Did user specify (just) time in locale's representation? */
      rem_d = strptime(dts, "%X", &tm);
      if (!rem_d) {
        memset(&tm, 0, sizeof(tm));
        /* Did user specify only hour, minute and seconds? */
        rem_d = strptime(dts, "%H:%M:%S", &tm);
        if (!rem_d) {
          memset(&tm, 0, sizeof(tm));
          /* Did user specify only hour and minute? */
          rem_d = strptime(dts, "%H:%M", &tm);

          if (!rem_d)
            /* Failed to parse date/time */
            return -1;
        }
      }

      /* User has only specified time, get yy/mm/dd from host, not adapter;
         assume date on host and adapter are the same. */
      now = time(NULL);
      loc_time = localtime(&now);
      tm.tm_year = loc_time->tm_year;
      tm.tm_mon = loc_time->tm_mon;
      tm.tm_mday = loc_time->tm_mday;
    }
  }

#if 0
  printf("rem_d   = '%s'\n", rem_d);
  printf("tm_year = %d\n", tm.tm_year);
  printf("tm_mon  = %d\n", tm.tm_mon);
  printf("tm_mday = %d\n", tm.tm_mday);
  printf("tm_hour = %d\n", tm.tm_hour);
  printf("tm_min  = %d\n", tm.tm_min);
  printf("tm_sec  = %d\n", tm.tm_sec);
#endif

  t = timegm(&tm);

  if (t == (time_t) -1)
    return -1;

  /* convert to 10-nsec ticks */
  *ts = t * 100000000ULL;
  return 0;
}

//
// main()
//
int main(int argc, const char* argv[])
{
  int status;
  char *ntpl = NULL;
  ThreadHandle_t hThread;     // Thread handle
  char errorBuffer[NT_ERRBUF_SIZE];  // Error buffer
  struct argparse argparse;

  DisplayProgramHeader();

  argparse_init(&argparse, arg_options, usageText, 0);
  argparse_parse(&argparse, argc, argv);

  if (opt_start_time != NULL) {
    if (ParseStartTime(opt_start_time, &absoluteStartTime) != 0) {
      fprintf(stderr, ">>> Error: \"-s %s\" time format is invalid\n", opt_start_time);
      return NT_APPL_ERROR_OPT_ARG;
    }
  }

  // Validate input parameters
  // Must always have a file.
  if (opt_file == NULL) {
    fprintf(stderr, "No file selected.\n");
    return NT_APPL_ERROR_OPT_MISSING;
  }

  // Either -s or -m is allowed. Not both.
  if ((modFactor != (uint32_t)-1) && (opt_start_time != NULL)) {
    fprintf(stderr, "Options -s and option -m are not allowed at the same time.\n");
    return NT_APPL_ERROR_OPT_CONFLICT;
  }

  if (modFactor == 0) {
    fprintf(stderr, "Option -m must be at least one.\n");
    return NT_APPL_ERROR_OPT_ARG;
  }
  if ((modFactor == (uint32_t)-1) && (opt_start_time == NULL)) {
    modFactor = 15U; /* set default value */
  }

  // Initialize the NTAPI library and thereby check if NTAPI_VERSION can be used together with this library
  if ((status = NT_Init(NTAPI_VERSION)) != NT_SUCCESS) {
    // Get the status code as text
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_Init() failed: %s\n", errorBuffer);
    appRunning = 0;
    exit(NT_APPL_ERROR_NTAPI);
  }

  // Check if the specified port is valid for our transmit purposes
  bool checksPassed = false;
  if ((status = CheckCapabilities(opt_port, &checksPassed)) != NT_SUCCESS) {
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "Adapter capability check failed: %s\n", errorBuffer);
    return NT_APPL_ERROR_NTAPI;
  } else if (!checksPassed) {
    return NT_APPL_ERROR_OPT_ARG;
  }

  printf("opt_path_delay: %d\n", opt_path_delay);

  // Get the timestamp of the first packet
  if (GetFileData() != 0) {
    return NT_APPL_ERROR_NTAPI;
  }


#if defined(WIN32) || defined(WIN64)
  CTR_SET_BEGIN(StopApplication, TRUE)
    fprintf(stderr, "Failed to register ctrl handler.\n");
    exit(NT_APPL_ERROR_OS);
  CTR_SET_END
#elif defined(__linux__) || defined(__FreeBSD__)
  CTR_SIG_BEGIN(SIGINT, StopApplication)
    fprintf(stderr, "Failed to register SIGINT sigaction.\n");
    exit(NT_APPL_ERROR_OS);
  CTR_SIG_END
  CTR_SIG_BEGIN(SIGTERM, StopApplication)
    fprintf(stderr, "Failed to register SIGTERM sigaction.\n");
    exit(NT_APPL_ERROR_OS);
  CTR_SIG_END
#endif


  if (tools_thread_create(&hThread, _ReplayData, ntpl) != 0) {
	fprintf(stderr, "Failed to start background receive thread.\n");
	exit(NT_APPL_ERROR_OS);
  }

  while (appRunning && !threadReady) {
		tools_sleep(1);
  }

  if (appRunning) {
	printf("Replaying file \"%s\"\n", opt_file);
  }
  while (appRunning) {
	tools_usleep(100000);

	if (timedTx4GA) {
	  uint64_t ts;
	  int64_t diff;
	  if (GetAdapterTimestamp(adapterNo, &ts) != NT_SUCCESS) {
		fprintf(stderr, "Reading adapter timestamp failed\n");
		appRunning = 0;
		return NT_APPL_ERROR_NTAPI;
	  }
	  if (txWait == 1) {
		diff = ((int64_t)((firstPacketTS + adapterTimeOffset) - ts + 10000) / 100000000);
		if (diff <= 0) {
		  txWait = 0;
		  continue;
		}
		printf("Throughput: %8.3f Mbps - Awaiting TX begin in %lld seconds            \r",
			   ((double)bytes*8)/100000.0, (long long int) diff);
	  }
	  else {
		if (opt_loop > 0) {
		  printf("Throughput: %8.3f Mbps - Awaiting tx completion            \r",
				 ((double)bytes*8)/100000);
		}
		else {
		  printf("Throughput: %8.3f Mbps                                     \r",
			((double)bytes*8)/100000);
		}
	  }
	  fflush(stdout);
	}
	else {
	  printf("Throughput: %8.3f Mbps\r", ((double)bytes*8)/1000000.0);fflush(stdout);
	}
	bytes=0;
  }
  tools_thread_join(hThread);

  printf("Done: %llu segments, %llu bytes                                                           \n", segments, totBytes);

  appRunning = 0;

  if (timedTx4GA) {
    if ((status = Clear4GATransmitOnTimestamp(adapterNo)) != NT_SUCCESS) {
      NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
      fprintf(stderr, "Disabling transmit on timestamp failed: %s\n", errorBuffer);
    }
  }

  // Close down the NTAPI library
  NT_Done();

  DisplayProgramFooter();
  return ApplExitCode;
}

//
// EOF
//
