#ifndef _PACKET
#define _PACKET

#include <winsock2.h>
#include <vector>

namespace Pawket
{
	namespace Packet
	{
		enum class Protocol
		{
			UNKNOWN,
			TCP,
			UDP,
			ICMP,
			IGMP,
			SCTP,
			OTHER
		};

		struct Endpoint
		{
			in_addr addr;
			WORD port;
		};

		enum class Direction
		{
			UNKNOWN,
			INCOMING,
			OUTGOING
		};
	}

	struct PACKET
	{
		Pawket::Packet::Protocol protocol {};
		Pawket::Packet::Endpoint source {};
		Pawket::Packet::Endpoint destination {};
		Pawket::Packet::Direction direction {};

		std::vector<char> payload;
	};
}

#endif