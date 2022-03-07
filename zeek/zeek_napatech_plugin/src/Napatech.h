/*
Copyright (c) 1995-2021, The Regents of the University of California
through the Lawrence Berkeley National Laboratory and the
International Computer Science Institute. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

(1) Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

(2) Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

(3) Neither the name of the University of California, Lawrence Berkeley
    National Laboratory, U.S. Dept. of Energy, International Computer
    Science Institute, nor the names of contributors may be used to endorse
    or promote products derived from this software without specific prior
    written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef IOSOURCE_PKTSRC_Napatech_SOURCE_H
#define IOSOURCE_PKTSRC_Napatech_SOURCE_H


extern "C" {
#include <stdio.h>
}

#include "iosource/PktSrc.h"
#include "iosource/pcap/Source.h"


#include <nt.h>
#include <errno.h>

#include "Cache.h"
#include "Napatech.bif.h"

// Napatech delivers timestamps in slices of 10ns. This is a convenience
// to convert 10ns slices into seconds
#define _NSEC_SLICE 100000000

//namespace iosource {
namespace zeek::iosource::pktsrc {

class NapatechSource : public iosource::PktSrc {
public:
	/*
	* Constructor
	* path: id of the stream to open
	*
	* is_live: Must be true, Napatech doesn't support an offline operation.
	*/
	NapatechSource(const std::string& path, bool is_live, const std::string& kind);

	/*
	* Destructor
	*/
	virtual ~NapatechSource();

static PktSrc* InstantiateNapatech(const std::string&path, bool is_live);

protected:
	/* PktSrc interface */
	virtual void Open();
	virtual void Close();
	virtual bool ExtractNextPacket(Packet* pkt);
	virtual void DoneWithPacket();
	virtual bool PrecompileFilter(int index, const std::string& filter);
	virtual bool SetFilter(int index);
	virtual void Statistics(Stats* stats);

private:

    static int32_t NTPL(const char *fmtStr, ...);

	Properties props;
	Stats stats;

	std::string kind;

	int current_filter;
	int stream_id;
	unsigned int num_received;
	unsigned int num_discarded;

	int status; // latest status code received from NTAPI
	char errorBuffer[NT_ERRBUF_SIZE]; // latest error string received from NTAPI

	NtNetStreamRx_t rx_stream; // Napatech stream
	NtNetBuf_t packet_buffer; // Net buffer container. Packet data is returned in this when calling NT_NetRxGet().
	NtDyn4Descr_t* packet_desc; // Current packet descriptor.

	// deduplication_cache holds a buffer of crc values to compare the currrent frame to
	// in order to look for duplicate frames seen on the wire
//	cache::lru<unsigned, unsigned> deduplication_cache = cache::lru<unsigned, unsigned>(BifConst::Napatech::dedupe_lru_size);

	NtStatStream_t stat_stream; // Napatech statistics stream
	NtStatistics_t nt_stat; // Napatech statistics data

	struct pcap_pkthdr current_hdr;
};

}
//}

#endif
