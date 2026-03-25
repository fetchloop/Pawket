#ifndef _IP_HEADER
#define _IP_HEADER

#include <Windows.h>

namespace Pawket
{
	namespace IP
	{
		#pragma pack(1)
		struct IP_HDR
		{
			BYTE version_header;
			BYTE service;
			WORD length;
			WORD id;
			WORD flags_fragment;
			BYTE TTL;
			BYTE protocol;
			WORD checksum;
			DWORD source_addr;
			DWORD dest_addr;
		};
		#pragma pack()

		#pragma pack(1)
		struct UDP
		{
			WORD source_port;
			WORD dest_port;
			WORD length;
			WORD checksum;
		};
		#pragma pack()

		#pragma pack(1)
		struct TCP
		{
			WORD source_port;
			WORD dest_port;
			DWORD sequence_num;
			DWORD ack_num;
			BYTE offset_reserve;
			BYTE flags;
			WORD window_size;
			WORD checksum;
			WORD urgent_ptr;
		};
		#pragma pack()

		inline BYTE get_header(const IP_HDR& ip_hdr)
		{
			return (ip_hdr.version_header & 0x0F) * 4;
		}

		inline BYTE get_version(const IP_HDR& ip_hdr)
		{
			return ip_hdr.version_header >> 4;
		}
	}
}

#endif