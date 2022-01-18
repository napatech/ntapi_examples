/*
 * Copyright 2021 Napatech A/S. All rights reserved.
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
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>

#include "nt.h"

#include "tool_def.h"
#include <addrswap_replay.h>
#include "tool_common.h"
#include <netinet/in.h>
#include <subst_parse.h>

#include "argparse.h"
#include "queue.h"



// #define DEBUG
#define ENABLE_VLAN


/******************************************************************************/
/* Internal variables                                                         */
/******************************************************************************/
static NtApplExitCode_t ApplExitCode = NT_APPL_SUCCESS;

static volatile int appRunning = 1;
static int threadReady = 0;

// Complete version string constant which can be searched for in the binary
static const char* progname = "addrswap_replay (v0.2)";
static unsigned long long bytes = 0;
static unsigned long long totBytes = 0;
static unsigned long long segments = 0;

static char* opt_file = NULL;
static int opt_port = -1;

static char* opt_substitute = NULL;

static int opt_numa = -1;

static uint64_t lastPacketTS = 0;
static uint64_t firstPacketTS = 0;
//static uint64_t firstPacketTSns = 0;
static uint64_t startTime = 0;
static uint64_t timeDelta = 0;

static NtNetFileType_e fileType;
static NtNetFileReadDesc_t fileDescriptorType;

static int adapterNo = -1;
static int numPorts = -1;
static uint64_t portMask = 0;
static int featureLevel = -1;
static enum NtProfileType_e profile;
static enum NtTimestampType_e adapterTimestampType;

static enum NtTxTimingMethod_e txTiming;


static uint64_t substitution_counter = 0;
static bool fcs_present_in_file = false;
/**
 * Table of valid options.
 */
struct argparse_option arg_options[] = {
		OPT_HELP(),
 		OPT_STRING('f',  "file",       &opt_file,       "Specifies the file to replay.", NULL, 0, 0, "file name"),
		OPT_INTEGER('p', "port",         &opt_port,     "Port on which a pcap file should be replayed.",
           				NULL, 0, 0, "port number"),
		OPT_INTEGER('n', "numa",         &opt_numa,
						"Numa node from which Tx host-buffer should be allocated.  The default\n"
						"is the Numa node associated with the slot in which the card is installed.",
           				NULL, 0, 0, "numa_node"),

		OPT_STRING('s', "substitute",
						&opt_substitute,       "Name of file specifying IP addresses to substitute.", NULL, 0, 0, "substiture_file name"),
						OPT_END(),
};

#define FCS_LENGTH 4

// The struct below represents the TX dynamic descriptor
// used by this application
//
// The struct is based on a copy of Dynamic descriptor 3
// It is modified with the location of the command bits
// for controlling L3/L4 checksum handling, time stamp
// inject handling and FCS handling.
//
// This example uses 12 bits of the color_hi field
// for these command bits. In other words - the
// normal color_hi:28 has been replaced by the
// 12 command bits and a color_hi_unused:16
//
// note: This example doesn't use the L3/L4 checksum
// command bits. See examples/net/checksum for an
// example of L3/L4 checksum control.
struct Dyn3_tx_descriptor {
	uint64_t capLength:14;
	uint64_t wireLength:14;
	uint64_t color_lo:14;
	uint64_t rxPort:6;
	uint64_t descrFormat:8;
	uint64_t descrLength:6;
	uint64_t tsColor:1;
	uint64_t ntDynDescr:1; // 6 byte

	uint64_t timestamp;  // 8 bytes - 14
	// Location of control bits in the packet descriptor:
	uint64_t frame_type:4;        // Frame type: 4 bits starting at bit offset 128
	uint64_t checksum_cmd:5;      // L3/L4 check sum command, 5 bits starting at bit offset 132
	uint64_t tsiCmd:1;            // Timestamp inject command, 1 bits starting at bit offset 137
	uint64_t fcsCmd:2;            // FCS command, 1 bits starting at bit offset 138
	uint64_t color_hi_unused:16;  // The remaing 16 unused bits of color_hi

	uint64_t offset0:10;
	uint64_t offset1:10;
};

/**
 * Valid values for layer 3 checksum command.
 */
enum ChecksumCmdLayer3 {
	CHECKSUM_LAYER3_DO_NOTHING = 0, // Do nothing
	CHECKSUM_LAYER3_BAD = 2,        // Insert BAD checksum in IPv4 header
	CHECKSUM_LAYER3_GOOD = 3,       // Insert GOOD checksum in IPv4 header
};

/**
 * Valid values for layer 4 checksum command.
 */
enum ChecksumCmdLayer4 {
	CHECKSUM_LAYER4_DO_NOTHING = 0,                        // Do nothing
	CHECKSUM_LAYER4_BAD = 2,                               // Insert BAD checksum in TCP/UDP header
	CHECKSUM_LAYER4_GOOD = 3,                              // Insert GOOD checksum in TCP/UDP header
	CHECKSUM_LAYER4_GOOD_TCP_ZERO_UDP = 4,                 // Insert GOOD checksum in TCP header and a value of ZERO in UDP header
	CHECKSUM_LAYER4_GOOD_TCP_ZERO_UDP_IPV4 = 5,            // Insert GOOD checksum in TCP header and a value of ZERO in UDP header when IP is IPv4
	CHECKSUM_LAYER4_GOOD_TCP_UDP_ZERO_UDP_TUNNEL = 6,      // Insert GOOD checksum in TCP/UDP header and a value of ZERO in UDP header when part of a tunnel
	CHECKSUM_LAYER4_GOOD_TCP_UDP_ZERO_UDP_IPV4_TUNNEL = 7, // Insert GOOD checksum in TCP/UDP header and a value of ZERO in UDP header when IP is IPv4 and part of a tunnel
};

enum FCSCmdLayer2 {
	// FCS command bits values
	FCS_GOOD      = 0,
	FCS_BAD       = 1,
	FCS_RESERVED  = 2,
	FCS_UNCHANGED = 3,
};


/**
 * Valid values for a descriptor frame type.
 *
 * Bit [  0]: Value 0: Not tunneled
 *            Value 1: Tunneled
 *
 * Bit [  1]: Value 0: IPv4 or other
 *            Value 1: IPv6
 *
 * Bit [3:2]: Value 0: Other
 *            Value 1: TCP
 *            Value 2: UDP
 *            Value 3: Reserved
 */
enum FrameType {
	FRAME_TYPE_IPV4_TCP          = 0x0 | 0x0 | 0x4,
	FRAME_TYPE_IPV4_UDP          = 0x0 | 0x0 | 0x8,
	FRAME_TYPE_IPV6_TCP          = 0x0 | 0x2 | 0x4,
	FRAME_TYPE_IPV6_UDP          = 0x0 | 0x2 | 0x8,
	FRAME_TYPE_IPV4_TCP_TUNNELED = 0x1 | 0x0 | 0x4,
	FRAME_TYPE_IPV4_UDP_TUNNELED = 0x1 | 0x0 | 0x8,
	FRAME_TYPE_IPV6_TCP_TUNNELED = 0x1 | 0x2 | 0x4,
	FRAME_TYPE_IPV6_UDP_TUNNELED = 0x1 | 0x2 | 0x8,
};

/**
 * Correctly combines layer 3 checksum command and layer 4 checksum command.
 */
static inline uint64_t checksum_command(uint32_t l3, uint32_t l4)
{
	return (((uint64_t)l3 & 0x3) << 3) | ((uint64_t)l4 & 0x7);
}

//=====================================================================
//
#define NT_DESCRIPTOR struct NtStd0Descr_s *

#ifdef DEBUG

static void HexDump(void *start, uint32_t count) {

	uint32_t lineno = 0;
	printf("%2d: ", lineno++);
	uint8_t *pCurr = (uint8_t *)start;
	for (uint32_t i = 0; i < count; ++i) {
		printf("0x%02x ", *(pCurr++));
		if (((i+1) % 16) == 0) {
			printf("\n");
			printf("%2d: ", lineno++);
		}
	}
	printf("\n");
}
#endif

struct ntpcap_ts_s {
	uint32_t sec;
	uint32_t usec;
};

struct ntpcap_hdr_s {
	struct ntpcap_ts_s ts;
	uint32_t caplen;
	uint32_t wirelen;
};


//
//=====================================================================

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
// Segment structure used to cache segments
//
struct Segment_s {
	STAILQ_ENTRY(Segment_s) le;
	uint64_t length;
	uint64_t pktWirelen;
	struct Dyn3_tx_descriptor descriptor;

	void *buf;
};

//
//
//
static bool is4GA(NtInfoStream_t hInfo, const int adapterNo)
{
	NtInfo_t infoMsg;
	static bool is4GA = false;

	// Get arch generation
	infoMsg.cmd = NT_INFO_CMD_READ_PROPERTY;
	snprintf(infoMsg.u.property.path, sizeof(infoMsg.u.property.path), "Adapter%d.FpgaGeneration", adapterNo);
	if (NT_InfoRead(hInfo, &infoMsg) == NT_SUCCESS) {
		is4GA = infoMsg.u.property.data.u.i >= 4;
	}
	return is4GA;
}

static bool checkDriverVersion(void)
{
	bool retval = false;
	NtInfoStream_t hInfo;           // Handle to an info stream
	NtInfo_t infoRead;

	NT_InfoOpen(&hInfo, "version");

	// Get driver version generation
	infoRead.cmd = NT_INFO_CMD_READ_SYSTEM;
	if (NT_InfoRead(hInfo, &infoRead) == NT_SUCCESS) {

		if (infoRead.u.system.data.version.major == 3) {
		   if (infoRead.u.system.data.version.minor >= 24) {
			   retval = true;
		   }
		} else if (infoRead.u.system.data.version.major > 3) {
			retval = true;
		}

		printf("system: %d.%d.%d\n",
				infoRead.u.system.data.version.major,
				infoRead.u.system.data.version.minor,
				infoRead.u.system.data.version.patch);
	}

	NT_InfoClose(hInfo);

	return retval;
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
	infoRead.u.port_v9.portNo =  portNo;
	if ((status = NT_InfoRead(hInfo, &infoRead)) != NT_SUCCESS) {
		NT_InfoClose(hInfo);
		return status;
	}

	numPorts = infoRead.u.port_v9.data.adapterInfo.numPorts;
	adapterNo = infoRead.u.port_v9.data.adapterNo;
	txTiming = infoRead.u.port_v9.data.adapterInfo.txTiming;
	adapterTimestampType = infoRead.u.port_v9.data.adapterInfo.timestampType;
	featureLevel = infoRead.u.port_v9.data.adapterInfo.featureLevel;
	profile = infoRead.u.port_v9.data.adapterInfo.profile;

#ifdef DEBUG
	printf("Adapter Timestamp Type: ");
	switch (adapterTimestampType) {
	case NT_TIMESTAMP_TYPE_NATIVE: printf("NT_TIMESTAMP_TYPE_NATIVE\n"); break;
	case NT_TIMESTAMP_TYPE_NATIVE_NDIS: printf("NT_TIMESTAMP_TYPE_NATIVE_NDIS\n"); break;
	case NT_TIMESTAMP_TYPE_NATIVE_UNIX: printf("NT_TIMESTAMP_TYPE_NATIVE_UNIX\n"); break;
	case NT_TIMESTAMP_TYPE_PCAP: printf("NT_TIMESTAMP_TYPE_PCAP\n"); break;
	case NT_TIMESTAMP_TYPE_PCAP_NANOTIME: printf("NT_TIMESTAMP_TYPE_PCAP_NANOTIME\n"); break;
	case NT_TIMESTAMP_TYPE_UNIX_NANOTIME: printf("NT_TIMESTAMP_TYPE_UNIX_NANOTIME\n"); break;
	default: printf("error: unknown timestamp type\n");
	}
#endif

	portMask = (uint64_t)1<<(portNo);
	printf("portNo: %d\n", portNo);

	if (!is4GA(hInfo, adapterNo)) {
		fprintf(stderr, "FPGA UNSUPPORTED - This program requires 4GA functionality.\n");
		return NT_ERROR_UNSUPPORTED_FPGA_MODULE;
	}

	for (uint8_t i = 0; i < numPorts; i++) {
		infoRead.cmd = NT_INFO_CMD_READ_PORT_V9;
		infoRead.u.port_v9.portNo = i;
		if ((status = NT_InfoRead(hInfo, &infoRead)) != NT_SUCCESS) {
			NT_InfoClose(hInfo);
			return status;
		}

		if (infoRead.u.port_v9.data.capabilities.featureMask&NT_PORT_FEATURE_RX_ONLY) {
			fprintf(stderr, "Transmit is not possible on all ports on the selected adapter: Port %d\n", i);
			fprintf(stderr, "All ports on the selected adapter must have transmit capabilities\n");
			NT_InfoClose(hInfo);
			return NT_SUCCESS;
		}
	}

	NT_InfoClose(hInfo);
	*checksPassed = true;
	return NT_SUCCESS;
}


static uint32_t GetFeatureLevel(const int adapterNo)
{
	int status;                     // Status variable
	NtInfoStream_t hInfo;           // Handle to an info stream
	NtInfo_t infoRead;


	if ((status = NT_InfoOpen(&hInfo, "info")) != NT_SUCCESS) {
		return status;
	}

	infoRead.cmd = NT_INFO_CMD_READ_ADAPTER_V6;
	infoRead.u.adapter_v6.adapterNo =  adapterNo;
	if ((status = NT_InfoRead(hInfo, &infoRead)) != NT_SUCCESS) {
		NT_InfoClose(hInfo);
		return status;
	}

	printf("featureLevel: 0x%x\n", infoRead.u.adapter_v6.data.featureLevel);
	switch(infoRead.u.adapter_v6.data.featureLevel) {
		case NT_FEATURE_LEVEL_N_ANL1:  printf("Analyser level 1\n"); break;
		case NT_FEATURE_LEVEL_N_ANL2:  printf("Analyser level 2\n"); break;
		case NT_FEATURE_LEVEL_N_ANL3:  printf("Analyser level 3\n"); break;
		case NT_FEATURE_LEVEL_N_ANL3A: printf("Analyser level 3A\n"); break;
		case NT_FEATURE_LEVEL_N_ANL4:  printf("Analyser level 4\n"); break;
		case NT_FEATURE_LEVEL_N_ANL5:  printf("Analyser level 5\n"); break;
		case NT_FEATURE_LEVEL_N_ANL6:  printf("Analyser level 6\n"); break;
		case NT_FEATURE_LEVEL_N_ANL7:  printf("Analyser level 7\n"); break;
		case NT_FEATURE_LEVEL_N_ANL8:  printf("Analyser level 8\n"); break;
		case NT_FEATURE_LEVEL_N_ANL9:  printf("Analyser level 9\n"); break;
		case NT_FEATURE_LEVEL_N_ANL10: printf("Analyser level 10\n"); break;
		case NT_FEATURE_LEVEL_N_ANL11: printf("Analyser level 11\n"); break;
		case NT_FEATURE_LEVEL_N_ANL12: printf("Analyser level 12\n"); break;
		case NT_FEATURE_LEVEL_N_ANL13: printf("Analyser level 13\n"); break;
		default: printf("unknown feature set\n");
	}

	NT_InfoClose(hInfo);
	return NT_SUCCESS;
}


//static int readInfo(NtNetStreamFile_t hNetFile, enum NtNetFileReadCmd_e cmd, NtNetFileRead_t* data)
//{
//	int status;
//	char errorBuffer[NT_ERRBUF_SIZE];
//
//	data->cmd = cmd;
//	if ((status = NT_NetFileRead(hNetFile, data)) != NT_SUCCESS) {
//		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
//		fprintf(stderr, "NtNetFileRead_t() failed: %s\n", errorBuffer);
//		return -1;
//	}
//
//	return 0;
//}

//static bool isNanotime(enum NtTimestampType_e tsType) {
//
//	return (tsType == NT_TIMESTAMP_TYPE_PCAP_NANOTIME) || (tsType == NT_TIMESTAMP_TYPE_UNIX_NANOTIME);
//}


static uint32_t vlanSize(struct MACHeader_s *mac) {
	uint32_t vlanSize = 0;
	uint16_t *typelen =  (uint16_t *)&(mac->mac_typelen);

	while (*typelen == ntohs(0x8100)) {
		vlanSize += VLAN_TAG_LEN;
		typelen += 2;  // skip over the VLAN ID to the next typelen field.
	}
	return vlanSize;
}


static int GetFileType(const NtNetStreamFile_t hNetFile) {
	NtNetFileRead_t data;
	int status;
	char errorBuffer[NT_ERRBUF_SIZE];

	data.cmd = NT_NETFILE_READ_FILETYPE_CMD;
	if ((status = NT_NetFileRead(hNetFile, &data)) != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NtNetFileRead_t() failed: %s\n", errorBuffer);
		return -1;
	}
	fileType = data.u.fileType;

	printf("Input fileType: ");
	switch(fileType) {
	case NT_NETFILE_TYPE_UNKNOWN: printf("    NT_NETFILE_TYPE_UNKNOWN\n"); break;
	case NT_NETFILE_TYPE_NT:      printf("    NT_NETFILE_TYPE_NT\n"); break;
	case NT_NETFILE_TYPE_PCAP:    printf("    NT_NETFILE_TYPE_PCAP\n"); break;
	case NT_NETFILE_TYPE_PCAP_NG: printf("    NT_NETFILE_TYPE_PCAP_NG\n"); break;
	default: printf("***** SHOULD NOT GET HERE *****\n");
	}

	data.cmd = NT_NETFILE_READ_DESCRIPTOR_CMD;
	if ((status = NT_NetFileRead(hNetFile, &data)) != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NtNetFileRead_t() failed: %s\n", errorBuffer);
		return -1;
	}
	fileDescriptorType = data.u.desc;



#ifdef DEBUG
	printf("File Timestamp Type: %d - ", adapterTimestampType);
	switch(fileDescriptorType.tsType) {
	case NT_TIMESTAMP_TYPE_NATIVE:			printf("    NT_TIMESTAMP_TYPE_NATIVE\n"); break;
	case NT_TIMESTAMP_TYPE_NATIVE_NDIS:		printf("    NT_TIMESTAMP_TYPE_NATIVE_NDIS\n"); break;
	case NT_TIMESTAMP_TYPE_NATIVE_UNIX: 	printf("    NT_TIMESTAMP_TYPE_NATIVE_UNIX\n"); break;
	case NT_TIMESTAMP_TYPE_PCAP: 			printf("    NT_TIMESTAMP_TYPE_PCAP\n"); break;
	case NT_TIMESTAMP_TYPE_PCAP_NANOTIME: 	printf("    NT_TIMESTAMP_TYPE_PCAP_NANOTIME\n"); break;
	case NT_TIMESTAMP_TYPE_UNIX_NANOTIME: 	printf("    NT_TIMESTAMP_TYPE_UNIX_NANOTIME\n"); break;
	default: printf("***** SHOULD NOT GET HERE *****\n");
	}

	printf("File Descriptor Type: ");
	switch(fileDescriptorType.desc) {
	case NT_PACKET_DESCRIPTOR_TYPE_UNKNOWN: 	printf("    NT_PACKET_DESCRIPTOR_TYPE_UNKNOWN\n"); break;
	case NT_PACKET_DESCRIPTOR_TYPE_PCAP: 		printf("    NT_PACKET_DESCRIPTOR_TYPE_PCAP\n"); break;
	case NT_PACKET_DESCRIPTOR_TYPE_NT: 			printf("    NT_PACKET_DESCRIPTOR_TYPE_NT\n"); break;
	case NT_PACKET_DESCRIPTOR_TYPE_NT_EXTENDED: printf("    NT_PACKET_DESCRIPTOR_TYPE_NT_EXTENDED\n"); break;
	case NT_PACKET_DESCRIPTOR_TYPE_DYNAMIC: 	printf("    NT_PACKET_DESCRIPTOR_TYPE_DYNAMIC\n"); break;
	default: printf("***** SHOULD NOT GET HERE *****\n");
	}

#endif

	return 0;
}

//
//
//
static int GetFileData(NtNetFileAttr_t *attr, const char *file_name)
{
	int status;                     // Status variable
	char errorBuffer[NT_ERRBUF_SIZE];

	NtNetStreamFile_t hNetFile;       // Handle to the File stream
	NtNetBuf_t hNetBufFile;           // Net buffer container. Used to return packets from the file stream
	struct NtNetBuf_s pktNetBuf;    // Packet netbuf structure.

	printf("Opening capture file %s\n", file_name);

	// Open the capture file to replay (captured with the capture example)
	if((status = NT_NetFileOpen_Attr(&hNetFile, opt_file, attr)) != NT_SUCCESS) {
		// Get the status code as text
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_NetFileOpen() failed: %s\n", errorBuffer);
		return -1;
	}

	GetFileType(hNetFile);

	// Read one segment from the file to get the first packet timestamp
	if ((status = NT_NetFileGet(hNetFile, &hNetBufFile)) != NT_SUCCESS) {
		if (status == NT_STATUS_END_OF_FILE) {
			fprintf(stderr, "The file %s has no data\n", "test.nt3gd");
			return -1;
		}
	}
	_nt_net_build_pkt_netbuf(hNetBufFile, &pktNetBuf);

	struct MACHeader_s* mac = (struct MACHeader_s*)(((void *)NT_NET_GET_PKT_L2_PTR(hNetBufFile)));
	struct IPv4Header_s* ip = NULL;

#ifdef ENABLE_VLAN
	uint32_t vlan_size = vlanSize(mac);
//	if (ntohs(mac->mac_typelen == ntohs(0x8100))) { // VLAN TAG is present
		ip = (struct IPv4Header_s*)(((void *)mac)+sizeof(struct MACHeader_s)+vlan_size);
//	} else {
//		ip = (struct IPv4Header_s*)(((void *)mac)+sizeof(struct MACHeader_s));
//	}
#else
	ip = (struct IPv4Header_s*)(((void *)mac)+sizeof(struct MACHeader_s));
#endif

	uint32_t pkt_size = ntohs(ip->ip_len) + sizeof(struct MACHeader_s);

	if ((pkt_size + vlan_size + FCS_LENGTH) < NT_NET_GET_PKT_WIRE_LENGTH_NT(hNetBufFile)) {
		fcs_present_in_file = true;
		printf("File includes FCS\n");
	} else {
		fcs_present_in_file = false;
		printf("File does not include FCS\n");
	}

//	firstPacketTSns = (isNanotime(fileDescriptorType.tsType) ? NT_NET_GET_PKT_TIMESTAMP(&pktNetBuf) : 10*NT_NET_GET_PKT_TIMESTAMP(&pktNetBuf));
	firstPacketTS = NT_NET_GET_PKT_TIMESTAMP(&pktNetBuf);

	// Close the file again. We will open it again later to actually transmit packets.
	NT_NetFileClose(hNetFile);

	return 0;
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
	*ts = configRead.u.timestampRead.data.nativeUnixTs * 10; // Convert 10ns ticks to 1ns

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
static uint32_t AwaitPacketTransmission(int adapterNo)
{
	int status;
	uint64_t ts;

	uint64_t totalTime = lastPacketTS - firstPacketTS;
	uint64_t stopTime =  startTime + totalTime;

#ifdef DEBUG
	printf("---- AwaitPacketTransmission() ----\n");
	printf("stopTime:   %ld\n\n", stopTime);
#endif

	while (appRunning) {
		// We need to wait for the packets to be transmitted before closing the stream and
		if ((status = GetAdapterTimestamp(adapterNo, &ts)) != NT_SUCCESS) {
			fprintf(stderr, "Reading adapter timestamp failed\n");
			return status;
		}

#ifdef DEBUG
		printf("adapter ts: %ld\n", ts);
#endif

		if (ts > stopTime) {
			break;
		}
		tools_sleep(1);
	}

	return NT_SUCCESS;
}

//
//
//
static uint32_t SetPortTransmitOnTimestamp(int portNo, uint64_t *timeDelta)
{
	uint32_t status;
	char errorBuffer[NT_ERRBUF_SIZE];       // Error buffer
	NtConfigStream_t hConfig;   // Config stream handle
	NtConfig_t configWrite;     // Config stream data container
	NtConfig_t configRead;     // Config stream data container

	// Open the config stream
	if ((status = NT_ConfigOpen(&hConfig, "Replay")) != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_ConfigOpen() failed: %s\n", errorBuffer);
		NT_ConfigClose(hConfig);
		return status;
	}

	// Read the adapter time
	configRead.parm = NT_CONFIG_PARM_ADAPTER_TIMESTAMP;
	configRead.u.timestampRead.adapter = adapterNo;
	if ((status = NT_ConfigRead(hConfig, &configRead)) != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_ConfigRead() failed: %s\n", errorBuffer);
		NT_ConfigClose(hConfig);
		return 0;
	}
	uint64_t adapterTS = configRead.u.timestampRead.data.nativeUnixTs * 10; // Convert 10ns ticks to 1ns

    // Calculate time delta - we add 1 second to give ourselves some headroom
    *timeDelta =   adapterTS
    		     - firstPacketTS
				 + 1000000000;

    startTime = adapterTS;

	configWrite.parm = NT_CONFIG_PARM_PORT_TRANSMIT_ON_TIMESTAMP;
	configWrite.u.transmitOnTimestamp.portNo = (uint8_t) portNo;
	configWrite.u.transmitOnTimestamp.data.timeDelta = *timeDelta;
	configWrite.u.transmitOnTimestamp.data.enable = true;
	configWrite.u.transmitOnTimestamp.data.forceTxOnTs = true;

	if ((status = NT_ConfigWrite(hConfig, &configWrite)) != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_ConfigWrite() failed: %s\n", errorBuffer);
		NT_ConfigClose(hConfig);
		return status;
	}

	// Close the config stream
	if ((status = NT_ConfigClose(hConfig)) != NT_SUCCESS) {
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

// struct and variable definition to be used in _ReplayData()
STAILQ_HEAD(, Segment_s) lhSegments; //!< List head. Maintain a list of cached segments

#ifdef DEBUG
//static uint32_t packet_num = 0;
#endif

static bool substitute_IP(uint8_t *pkt) {
	bool found = false;

	struct MACHeader_s* mac = (struct MACHeader_s*)(((void *)pkt));
	struct IPv4Header_s* ip;

#ifdef ENABLE_VLAN
//	if (ntohs(mac->mac_typelen == ntohs(0x800))) {
//		ip = (struct IPv4Header_s*)(((void *)mac)+sizeof(struct MACHeader_s));
//	} else if (ntohs(mac->mac_typelen == ntohs(0x8100))) {
		ip = (struct IPv4Header_s*)(((void *)mac)+sizeof(struct MACHeader_s)+vlanSize(mac));
//	} else {
//		printf("Unknown eth_type (stacked vlan perhaps?) - skipping substitution.\n");
//		return false;
//	}
#else
	ip = (struct IPv4Header_s*)(((void *)mac)+sizeof(struct MACHeader_s));
#endif

	int new_src = search_addr_map(ip->ip_src);
	if (new_src >= 0) {
		found = true;
		ip->ip_src = new_src;
	}

	int new_dst = search_addr_map(ip->ip_dst);
	if (new_dst >= 0) {
		found = true;
		ip->ip_dst = new_dst;
	}
	return found;
}

static bool setup_tx(NtNetStreamTx_t *hNetTx, uint32_t port) {
	char errorBuffer[NT_ERRBUF_SIZE];           // Error buffer
	int status;                     // Status variable
	NtInfoStream_t hInfo;           // Handle to a info stream
	NtInfo_t infoRead;              // Buffer to hold data from infostream
	NtNetTxAttr_t txAttr;

	// Initialize TX attributes struct and set up relevant stream related attributes.
	NT_NetTxOpenAttrInit(&txAttr);
	NT_NetTxOpenAttrSetName(&txAttr, "replay_tx");
	NT_NetTxOpenAttrSetPortMask(&txAttr, 1 << port);
	NT_NetTxOpenAttrSetTimestampType(&txAttr, NT_TIMESTAMP_TYPE_UNIX_NANOTIME);


	if (opt_numa == -1) {
		NT_NetTxOpenAttrSetNUMA(&txAttr, NT_NETTX_NUMA_ADAPTER_HB);
		printf("Using default NUMA node.\n");
	} else {
		NT_NetTxOpenAttrSetNUMA(&txAttr, opt_numa);
		printf("Using NUMA node %d\n", opt_numa);
	}

	// Setting descriptor mode to DYN3 will cause Net Buffers received from NT_NetTxGet
	// to be initialized with a DYN3 packet descriptor.
	if((status = NT_NetTxOpenAttrSetDescriptorMode(&txAttr, NT_NETTX_DESCRIPTOR_MODE_DYN3)) != NT_SUCCESS){
		// Get the status code as text
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_NetTxOpenAttrSetDescriptorMode() failed: %s\n", errorBuffer);
		return false;
	}

	// To tell the adapter where to look for the frame type and checksum command bits.
	if ((status = NT_NetTxOpenAttrSetDescriptorPosFrameType(&txAttr, true, 128)) != NT_SUCCESS) {
		// Get the status code as text
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "to() failed: %s\n", errorBuffer);
		return false;
	}
	if ((status = NT_NetTxOpenAttrSetDescriptorPosChecksumCmd(&txAttr, true, 132)) != NT_SUCCESS){
		// Get the status code as text
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_NetTxOpenAttrSetDescriptorPosChecksumCmd() failed: %s\n", errorBuffer);
		return false;
	}

	status = NT_NetTxOpenAttrSetTxtDescriptorPosFcs(&txAttr,  true, 138);
	if (status != NT_SUCCESS)
	{
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_NetTxOpenAttrSetTxtDescriptorPosFcs failed: %s\n", errorBuffer);
		return false;
	}

	// Open the infostream.
	if((status = NT_InfoOpen(&hInfo, "replay")) != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_InfoOpen() failed: %s\n", errorBuffer);
		return false;
	}

	// Check whether or not TX is supported on the defined port
	infoRead.cmd = NT_INFO_CMD_READ_PORT_V9;
	infoRead.u.port_v9.portNo=port;  // TX port is hardcoded to defined PORT
	if((status = NT_InfoRead(hInfo, &infoRead)) != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_InfoRead() failed: %s\n", errorBuffer);
		NT_InfoClose(hInfo);
		return false;
	}

	// save adapter number of the TX port
	adapterNo = infoRead.u.port_v9.data.adapterNo ;

	if(infoRead.u.port_v9.data.capabilities.featureMask&NT_PORT_FEATURE_RX_ONLY) {
		fprintf(stderr, "ERROR: The selected port (%d) is Rx only.\n",port);
		NT_InfoClose(hInfo);
		return false;
	}

	uint32_t checksum_mask = NT_PORT_FEATURE_IPV4_TX_CHECKSUM_CALC | NT_PORT_FEATURE_UDP_TX_CHECKSUM_CALC | NT_PORT_FEATURE_TCP_TX_CHECKSUM_CALC;
	if((infoRead.u.port_v9.data.capabilities.featureMask&checksum_mask) == 0) {
		fprintf(stderr, "ERROR: The selected port does not support checksum generation.\n");
		fprintf(stderr, "This program requires an FPGA image with support for the\n");
		fprintf(stderr, "Napatech Test and Measurement feature set.\n");
		return false;
	}

	// Check whether or not absolute TX timing is supported
	adapterNo = infoRead.u.port_v9.data.adapterNo ;
	infoRead.cmd = NT_INFO_CMD_READ_ADAPTER_V6;
	infoRead.u.adapter_v6.adapterNo=adapterNo;  // Adapter is derived from PORT of adapter
	if((status = NT_InfoRead(hInfo, &infoRead)) != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_InfoRead() failed: %s\n", errorBuffer);
		NT_InfoClose(hInfo);
		return false;
	}

	NT_InfoClose(hInfo);

	status = NT_NetTxOpen_Attr(hNetTx, &txAttr);
	if (status != NT_SUCCESS) {
		appRunning = 0;
		// Get the status code as text
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_NetTxOpen() failed: %s\n", errorBuffer);
		ApplExitCode = NT_APPL_ERROR_NTAPI;
		return false;
	}

	return true;
}


//
// PThread handler function declaration
//
static THREAD_API _Replay(void* arg)
{
	(void)arg;

	int status;                     // Status variable
	char errorBuffer[NT_ERRBUF_SIZE];           // Error buffer
//	uint32_t numPackets=0;               // The number of packets replayed
//	uint32_t numBytes=0;                 // The number of bytes replayed

	NtNetStreamFile_t hNetFile;       // Handle to the File stream
	NtNetBuf_t hNetBufFile;           // Net buffer container. Used to return packets from the file stream
	NtNetStreamTx_t hNetTx;           // Handle to the TX stream
	NtNetBuf_t hNetBufTx;             // Net buffer container. Used when getting a transmit buffer
	uint64_t pktCount = 0;
	time_t lastTime = time(NULL);

#define MAX_NUM_TXPORTS (32)        // Max number of txport in packet descriptor (2^5)

	NtNetFileAttr_t attr;
	NT_NetFileOpenAttrInit(&attr);
	NT_NetFileOpenAttrSetEnablePcapToNtConversion(&attr, true);
	NT_NetFileOpenAttrSetName(&attr, "FileStream");
	NT_NetFileOpenAttrSetInterface(&attr, NT_NET_INTERFACE_PACKET);

	//NT_NetFileOpenAttrSetConvertedTsType(&attr, NT_TIMESTAMP_TYPE_NATIVE_UNIX);
	NT_NetFileOpenAttrSetConvertedTsType(&attr, NT_TIMESTAMP_TYPE_UNIX_NANOTIME);

	// Get the timestamp of the first packet
	if (GetFileData(&attr, opt_file) != 0) {
		THREAD_RETURN_OK;
	}

	if (!setup_tx(&hNetTx, opt_port)) {
		appRunning = 0;
		ApplExitCode = NT_APPL_ERROR_CONFIG;
		THREAD_RETURN_OK;
	}

	// Open the capture file to replay (captured with the capture example)
	if ((status = NT_NetFileOpen_Attr(&hNetFile, opt_file, &attr)) != NT_SUCCESS) {
		// Get the status code as text
		appRunning = 0;
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_NetFileOpen() failed: %s\n", errorBuffer);
		ApplExitCode = NT_APPL_ERROR_NTAPI;
		THREAD_RETURN_OK
	}

	// Activate transmit on timestamp.
	timeDelta = 0;
	status = SetPortTransmitOnTimestamp(opt_port, &timeDelta);
	if (status != NT_SUCCESS) {
		appRunning = 0;
		fprintf(stderr, "Setup of transmit on timestamp failed\n");
		ApplExitCode = NT_APPL_ERROR_NTAPI;
		THREAD_RETURN_OK
	}

	totBytes=0;
	threadReady = 1;

	// Get packet segements from the file and transmit them
	while (appRunning) {
		// Get the packet
		if((status = NT_NetFileGet(hNetFile, &hNetBufFile)) != NT_SUCCESS) {
			if(status == NT_STATUS_END_OF_FILE) {
				// The file has no more data
				break;
			}
			// Get the status code as text
			NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
			fprintf(stderr, "NT_NetFileGet() failed: %s\n", errorBuffer);
			THREAD_RETURN_OK;
		}

		NtNetFileRead_t fileRead;
		fileRead.cmd = NT_NETFILE_READ_INFO_CMD_V1;
		if ((status = NT_NetFileRead(hNetFile, &fileRead)) != NT_SUCCESS) {
			NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
			fprintf(stderr, "NT_NetfileRead() failed: %s\n", errorBuffer);
			THREAD_RETURN_OK;
		}

		uint32_t pktWirelen = NT_NET_GET_PKT_WIRE_LENGTH_NT(hNetBufFile);

		if (fcs_present_in_file) {
			pktWirelen -= FCS_LENGTH;
		}

		do {
			status = NT_NetTxGet(hNetTx, &hNetBufTx,
					opt_port, (pktWirelen < 64 ? 64 : pktWirelen),
					NT_NETTX_PACKET_OPTION_DYN, 100);
	    } while ((appRunning == 1) && (status == NT_STATUS_TIMEOUT || status == NT_STATUS_TRYAGAIN));

		if (status != NT_SUCCESS) {
			// Get the status code as text
			NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
			fprintf(stderr, "ncr-NT_NetTxGet() failed: %s\n", errorBuffer);
			THREAD_RETURN_OK;
		}

		// Get a pointer to the beginning of the Ethernet frame, and copy Test_packet to that memory.
		uint8_t *dest_pkt = NT_NET_GET_PKT_L2_PTR(hNetBufTx);
		memcpy(dest_pkt, NT_NET_GET_PKT_L2_PTR(hNetBufFile), pktWirelen);

		struct NtDyn3Descr_s *dest_descriptor = NT_NET_GET_PKT_DESCR_PTR_DYN3(hNetBufTx);
		struct Dyn3_tx_descriptor *overlay_ptr = (struct Dyn3_tx_descriptor *)dest_descriptor;

		// Check if there is one or more VLAN tags
		struct MACHeader_s* mac = (struct MACHeader_s*)dest_pkt;
		uint32_t vlan_size = vlanSize(mac);

		struct IPv4Header_s* pIPv4 = (struct IPv4Header_s*)(((void *)mac)+sizeof(struct MACHeader_s) + vlan_size);

		// Make sure it's IPv4 and not IPv6 or another pcol
		if (pIPv4->ip_v == 4) {
			// Set up to recalculate the IP and UDP/TCP checksums
			dest_descriptor->offset0 = sizeof(struct MACHeader_s) + vlan_size;  // Point to start of Layer 3,
			dest_descriptor->offset1 = sizeof(struct MACHeader_s) + sizeof(struct IPv4Header_s) + vlan_size;   // Point to start of Layer 4

			// Set checksum values via the overlay struct.
			switch(pIPv4->ip_prot) {
			case IP_PROT_UDP:
				overlay_ptr->frame_type = FRAME_TYPE_IPV4_UDP & 0xF;
				break;
			case IP_PROT_TCP:
				overlay_ptr->frame_type = FRAME_TYPE_IPV4_TCP & 0xF;
				break;
			};
			overlay_ptr->checksum_cmd = checksum_command(CHECKSUM_LAYER3_GOOD, CHECKSUM_LAYER4_GOOD) & 0x1F;
		} else {
			// we won't touch it if it's not IPv4
			overlay_ptr->checksum_cmd = checksum_command(CHECKSUM_LAYER3_DO_NOTHING, CHECKSUM_LAYER4_DO_NOTHING) & 0x1F;
		}

		overlay_ptr->fcsCmd = FCS_GOOD & 0x3;
		dest_descriptor->timestamp = lastPacketTS = NT_NET_GET_PKT_TIMESTAMP(hNetBufFile);

		segments++;
		bytes    += pktWirelen; //NT_NET_GET_SEGMENT_LENGTH(hNetBufFile);
		totBytes += pktWirelen; // NT_NET_GET_SEGMENT_LENGTH(hNetBufFile);


		if (opt_substitute != NULL) {
			if (substitute_IP(dest_pkt)) {
				++substitution_counter;
#ifdef DEBUG
				printf("new  src->dst: "); PrintIP(pIPv4->ip_src); printf("->");PrintIP(pIPv4->ip_dst); printf("\n");
#endif
			}
		}

#ifdef DEBUG
		//if (pktCount == 0) {
			printf("pktCount: %ld  ", pktCount);
			printf("timestamp: %ld\n",  dest_descriptor->timestamp);
			//HexDump(dest_descriptor, NT_DYNAMIC_DESCRIPTOR_03_LENGTH + pktWirelen);
			//printf("\n");
		//}
#endif

		++pktCount;
		// Release the TX buffer and the packets within the segment will be transmitted
		if((status = NT_NetTxRelease(hNetTx, hNetBufTx)) != NT_SUCCESS) {
			// Get the status code as text
			NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
			fprintf(stderr, "NT_NetTxRelease() failed: %s\n", errorBuffer);
			THREAD_RETURN_OK;
		}
		// Release the file packet
		if((status = NT_NetFileRelease(hNetFile, hNetBufFile)) != NT_SUCCESS) {
			// Get the status code as text
			NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
			fprintf(stderr, "NT_NetFileRelease() failed: %s\n", errorBuffer);
			THREAD_RETURN_OK;
		}

        time_t currTime = time(NULL);
		if (currTime > lastTime) {
			printf("Delivered %ld pkts to card...\r", pktCount);
			fflush(stdout);
			lastTime = currTime;
		}
	}
	printf("Delivered %ld pkts to card...\n", pktCount);

	// Close the file stream
	NT_NetFileClose(hNetFile);

	if (AwaitPacketTransmission(adapterNo) != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "Waiting for transmission to end failed: %s\n", errorBuffer);
	}

	if ((status = ClearPortTransmitOnTimestamp(opt_port)) != NT_SUCCESS) {
		appRunning = 0;
		fprintf(stderr, "Disabling transmit on timestamp failed\n");
		ApplExitCode = NT_APPL_ERROR_NTAPI;
		THREAD_RETURN_OK
	}
	NT_NetTxClose(hNetTx);


	appRunning = 0;
	THREAD_RETURN_OK
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

	// Validate input parameters
	// Must always have a file.
	if (opt_file == NULL) {
		fprintf(stderr, "%s\n", *usageText);
		return NT_APPL_ERROR_OPT_MISSING;
	}

	if (opt_port == -1) {
		fprintf(stderr, "%s\n", *usageText);
		return NT_APPL_ERROR_OPT_MISSING;
	}


	if (opt_substitute != NULL) {
		printf("loading substitute addresses from %s\n", opt_substitute);
		load_substitute_addr_map(opt_substitute);
		printf("\n");
	}

	// Initialize the NTAPI library and thereby check if NTAPI_VERSION can be used together with this library
	if ((status = NT_Init(NTAPI_VERSION)) != NT_SUCCESS) {
		// Get the status code as text
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "NT_Init() failed: %s\n", errorBuffer);
		appRunning = 0;
		exit(NT_APPL_ERROR_NTAPI);
	}

	GetFeatureLevel(0);
//	NT_Done();
//	exit (0);


	if (!checkDriverVersion() ) {
		printf("Driver version 3.24 or greater required.\n");
		exit (NT_APPL_ERROR_NTAPI);
	}

	// Check if the specified port is valid for our transmit purposes
	bool checksPassed = true; // false;
	if ((status = CheckCapabilities(opt_port, &checksPassed)) != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "Adapter capability check failed: %s\n", errorBuffer);
		return NT_APPL_ERROR_NTAPI;
	} else if (!checksPassed) {
		return NT_APPL_ERROR_OPT_ARG;
	}


	CTR_SIG_BEGIN(SIGINT, StopApplication)
	fprintf(stderr, "Failed to register SIGINT sigaction.\n");
	exit(NT_APPL_ERROR_OS);
	CTR_SIG_END
	CTR_SIG_BEGIN(SIGTERM, StopApplication)
	fprintf(stderr, "Failed to register SIGTERM sigaction.\n");
	exit(NT_APPL_ERROR_OS);
	CTR_SIG_END

	if (tools_thread_create(&hThread, _Replay, ntpl) != 0) {
		fprintf(stderr, "Failed to start background receive thread.\n");
		exit(NT_APPL_ERROR_OS);
	}

	while (appRunning && !threadReady) {
		tools_sleep(1);
	}

	tools_thread_join(hThread);

	if (ApplExitCode == NT_APPL_SUCCESS) {
		printf("Tx Complete - sent: %llu packets, %llu bytes\n", segments, totBytes);
	} else {
		printf("Program terminated!\n");
	}

	if ((status = ClearPortTransmitOnTimestamp(opt_port)) != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		fprintf(stderr, "Disabling transmit on timestamp failed: %s\n", errorBuffer);
	}

	// Close down the NTAPI library
	NT_Done();

	delete_addr_map();

	DisplayProgramFooter();
	return ApplExitCode;
}

//
// EOF
//
