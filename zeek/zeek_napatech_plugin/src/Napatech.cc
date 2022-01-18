
#include "zeek-config.h"

#include "Napatech.h"
#include "Napatech.bif.h"
#include "Cache.h"
#include <signal.h>

#include <iostream>
#include <fstream>

#include <nt.h>

using namespace zeek::iosource::pktsrc;


NapatechSource::~NapatechSource()
{
	Close();
}

NapatechSource::NapatechSource(const std::string& path, bool is_live, const std::string& arg_kind)
{
	if ( ! is_live )
		Error("napatech source does not support offline input");

	kind = arg_kind;
	current_filter = -1;

	stream_id = atoi(path.c_str());

	props.path = path;
	props.is_live = is_live;

        status = NT_Init(NTAPI_VERSION);
        if ( status != NT_SUCCESS ) {
                NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
                Error(errorBuffer);
                return;
        }

}



void NapatechSource::Open()
{

	printf("NapatechSource::Open() - stream_id: %d\n", stream_id);

//	uint64_t test_value = zeek::BifConst::Napatech::test_value;
//	printf("test_value: %ld\n", test_value);

//	// Last argument is the HBA - Host Buffer Allowance, or the amount of the Host Buffer that
	// can be held back. Need to createa  BIF to configure this.
	status = NT_NetRxOpen(&rx_stream, "BroStream", NT_NET_INTERFACE_PACKET, stream_id, -1);
	if ( status != NT_SUCCESS) {
		Info("Failed to open stream");
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		Error(errorBuffer);
		return;
	}

	props.netmask = NETMASK_UNKNOWN;
	props.is_live = true;
	props.link_type = DLT_EN10MB;

	// Open a statistics stream
	// Napatech NICs track what gets collected from a stream and what does not.
	// Because of this, we can move lots of the stats tracking out of the plugin.
	status = NT_StatOpen(&stat_stream, "BroStats");
	if ( status != NT_SUCCESS ) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		Error(errorBuffer);
		return;
	}

	// Read Statistics from Napatech API
	nt_stat.cmd=NT_STATISTICS_READ_CMD_QUERY_V2;
	nt_stat.u.query_v2.poll=0; // Get a new dataset
	nt_stat.u.query_v2.clear=1; // Clear the counters for this read

	// Do a dummy read of the stats API to clear the counters
	status = NT_StatRead(stat_stream, &nt_stat);
	if ( status != NT_SUCCESS ) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		Error(errorBuffer);
		return;
	}

	nt_stat.u.query_v2.clear=0; // Don't clear the counters again

	Opened(props);
}


void NapatechSource::Close()
{

	if (props.is_live) {
		// Close configuration stream, release the host buffer, and remove all assigned NTPL assignments.
		NT_NetRxClose(rx_stream);

		// Close the network statistics stream
		status = NT_StatClose(stat_stream);
		if ( status != NT_SUCCESS ) {
					NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
					Error(errorBuffer);
					return;
		}

		NT_Done();
		props.is_live = false;
	}
}

static inline struct timeval nt_timestamp_to_timeval(const int64_t ts)
{
    struct timeval tv;

    if (ts == 0) {
        return (struct timeval) { 0, 0 };
    } else {
        tv.tv_sec = ts / _NSEC_SLICE;
		tv.tv_usec = ( ts % _NSEC_SLICE ) / 100;
    }

    return tv;
}


static uint64_t pkt_count = 0;

bool NapatechSource::ExtractNextPacket(Packet* pkt)
{

	u_char* data;
	while (true) {
		status = NT_NetRxGet(rx_stream, &packet_buffer, 1000);

		if (status != NT_SUCCESS) {
			if ((status == NT_STATUS_TIMEOUT) || (status == NT_STATUS_TRYAGAIN)) {
				// wait a little longer for a packet
				return false;
			}
			NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
			Info(errorBuffer);
			return false;
		}

//		printf("pkt_count: %ld\r", ++pkt_count);fflush(stdout);


		current_hdr.ts = nt_timestamp_to_timeval(NT_NET_GET_PKT_TIMESTAMP(packet_buffer));

		current_hdr.caplen = NT_NET_GET_PKT_CAP_LENGTH(packet_buffer);
		current_hdr.len = NT_NET_GET_PKT_WIRE_LENGTH(packet_buffer);
		data = (unsigned char *) NT_NET_GET_PKT_L2_PTR(packet_buffer);

		if ( ! ApplyBPFFilter(current_filter, &current_hdr, data) ) {
			DoneWithPacket();
			continue;
		}

		pkt->Init(props.link_type, &current_hdr.ts, current_hdr.caplen, current_hdr.len, data);
		++stats.received;
		stats.bytes_received += current_hdr.len;
		return true;
	}

	// Should never reach this point
	return false;
}

void NapatechSource::DoneWithPacket()
{
	// release the current packet
	status = NT_NetRxRelease(rx_stream, packet_buffer);
	if (status != NT_SUCCESS) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		Info(errorBuffer);
	}
}

bool NapatechSource::PrecompileFilter(int index, const std::string& filter)
{
    return PktSrc::PrecompileBPFFilter(index, filter);
}

bool NapatechSource::SetFilter(int index)
{
    current_filter = index;

    return true;
}

void NapatechSource::Statistics(Stats* s)
{
	printf("Statistics\n");
	// Grab the counter from this plugin for how much it has seen.
	s->received = stats.received;
	s->bytes_received = stats.bytes_received;

	status = NT_StatRead(stat_stream, &nt_stat);
	if ( status != NT_SUCCESS ) {
		NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
		Error(errorBuffer);
		return;
	}

	// Set counters from NTAPI returns
	s->dropped = nt_stat.u.query_v2.data.stream.streamid[stream_id].drop.pkts;
	s->link = nt_stat.u.query_v2.data.stream.streamid[stream_id].forward.pkts;

}

zeek::iosource::PktSrc* NapatechSource::InstantiateNapatech(const std::string& path, bool is_live)
{
	return new NapatechSource(path, is_live, "napatech");
}
