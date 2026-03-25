#ifndef _PAWKET_LIST
#define _PAWKET_LIST

#include "packet.h"

#include <mutex>
#include <vector>

namespace Pawket
{
	namespace Packet
	{
		namespace List
		{
			inline std::vector<Pawket::PACKET> packet_list;
			inline std::mutex list_mutex;
		}
	}
}

#endif