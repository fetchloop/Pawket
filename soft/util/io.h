#ifndef _EXPORT_H
#define _EXPORT_H

#include <filesystem>
#include <vector>

#include <ShlObj.h>
#include <Commdlg.h>

#include "packet.h"
#include "time.h"
#include "../handler.h"

#pragma pack(1)
struct PcapGlobalHeader
{
    DWORD magic_number = 0xA1B2C3D4; // microsecond resolution magic
    WORD version_major = 2;
    WORD version_minor = 4;
    DWORD thiszone = 0; // GMT no offset
    DWORD sigfigs = 0; // Accuracy of timestamps, 0 usuall fine
    DWORD snaplen = 65535; // Max packet length
    DWORD network = 101; // LINKTYPE_RAW
};
#pragma pack()

#pragma pack(1)
struct PcapRecordHeader
{
    DWORD ts_sec; // Timestamp seconds
    DWORD ts_usec; // Timestamp microseconds
    DWORD incl_len; // Length of data (size of packet.raw)
    DWORD orig_len;
};
#pragma pack()

namespace Pawket
{
	namespace Pcap
	{
        // Writes a packet capture file, into %appdata%/Pawket/Exports
        void export_pcap(const std::vector<Pawket::PACKET>& packets)
        {
            // Build the exports directory path
            wchar_t appdata_path[MAX_PATH];
            SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, appdata_path);

            std::filesystem::path exports_dir = std::filesystem::path(appdata_path) / "Pawket" / "Exports";
            std::filesystem::create_directories(exports_dir);

            // Build the filename from the current time
            std::string filename = "pawket_" + Pawket::Time::get_win_string_timestamp(std::chrono::system_clock::now()) + ".pcap";
            std::filesystem::path export_path = exports_dir / filename;

            std::ofstream file(export_path, std::ios::binary);
            if (!file.is_open()) return;

            // Write the global header
            PcapGlobalHeader global_header{};
            file.write(reinterpret_cast<const char*>(&global_header), sizeof(PcapGlobalHeader));

            for (const Pawket::PACKET& p : packets)
            {
                // Build the record header from the packets timestamp and raw frame size
                auto epoch = p.timestamp.time_since_epoch();
                auto secs = std::chrono::duration_cast<std::chrono::seconds>(epoch);
                auto usecs = std::chrono::duration_cast<std::chrono::microseconds>(epoch) - secs;

                PcapRecordHeader record{};
                record.ts_sec = (DWORD)secs.count();
                record.ts_usec = (DWORD)usecs.count();
                record.incl_len = (DWORD)p.raw.size();
                record.orig_len = (DWORD)p.raw.size();

                // Write the record header then the raw frame bytes
                file.write(reinterpret_cast<const char*>(&record), sizeof(PcapRecordHeader));
                file.write(p.raw.data(), p.raw.size());
            }
        }

		// Writes a json file of captured packets, into %appdata%/Pawket/Exports
        void export_json(const std::vector<Pawket::PACKET>& packets)
        {
            // Build the exports directory path
            wchar_t appdata_path[MAX_PATH];
            SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, appdata_path);

            std::filesystem::path exports_dir = std::filesystem::path(appdata_path) / "Pawket" / "Exports";
            std::filesystem::create_directories(exports_dir);

            // Build the filename from the current time
            std::string filename = "pawket_" + Pawket::Time::get_win_string_timestamp(std::chrono::system_clock::now()) + ".json";
            std::filesystem::path export_path = exports_dir / filename;

            std::ofstream file(export_path);
            if (!file.is_open()) return;

            char src[INET_ADDRSTRLEN]{};
            char dst[INET_ADDRSTRLEN]{};

            file << "[\n";
            for (size_t i = 0; i < packets.size(); i++)
            {
                const Pawket::PACKET& p = packets[i];

                inet_ntop(AF_INET, &p.source.addr, src, INET_ADDRSTRLEN);
                inet_ntop(AF_INET, &p.destination.addr, dst, INET_ADDRSTRLEN);

                // Build the payload hex string from the raw data
                std::string hex;
                hex.reserve(p.length * 3);
                const char* payload_start = p.raw.data() + p.offset;
                char byte_buf[4]{};
                for (size_t b = 0; b < p.length; b++)
                {
                    snprintf(byte_buf, sizeof(byte_buf), "%02X ", (BYTE)payload_start[b]);
                    hex += byte_buf;
                }
                if (!hex.empty()) hex.pop_back(); // Remove trailing space

                file << "  {\n";
                file << "    \"timestamp\": \"" << Pawket::Time::get_string_timestamp(p.timestamp) << "\",\n";
                file << "    \"protocol\": \"" << Pawket::Handler::get_packet_protocol_string(p) << "\",\n";
                file << "    \"source_ip\": \"" << src << "\",\n";
                file << "    \"source_port\": " << p.source.port << ",\n";
                file << "    \"destination_ip\": \"" << dst << "\",\n";
                file << "    \"destination_port\": " << p.destination.port << ",\n";
                file << "    \"direction\": \"" << (p.direction == Pawket::Packet::Direction::INCOMING ? "incoming" : "outgoing") << "\",\n";
                file << "    \"payload\": \"" << hex << "\"\n";
                file << "  }" << (i + 1 < packets.size() ? "," : "") << "\n";
            }
            file << "]\n";
        }

		// Reads a pcap file into the software.
		bool import(HWND hwnd)
		{
			// Open the windows file picker filtered to .pcap files
			char file_path[MAX_PATH]{};

			OPENFILENAMEA ofn{};
			ofn.lStructSize = sizeof(OPENFILENAMEA);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFilter = "PCAP Files\0*.pcap\0All Files\0*.*\0";
			ofn.lpstrFile = file_path;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrTitle = "Import PCAP";
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

			if (!GetOpenFileNameA(&ofn)) return false;

			std::ifstream file(file_path, std::ios::binary);
			if (!file.is_open()) return false;

			// Read and validate the global header
			PcapGlobalHeader global_header{};
			file.read(reinterpret_cast<char*>(&global_header), sizeof(PcapGlobalHeader));

			if (global_header.magic_number != 0xA1B2C3D4) return false;
			if (global_header.network != 101) return false;

			// Read each packet record
			PcapRecordHeader record{};
			while (file.read(reinterpret_cast<char*>(&record), sizeof(PcapRecordHeader)))
			{
				if (record.incl_len == 0 || record.incl_len > 65535) continue;

				// Read the raw frame bytes
				std::vector<char> raw(record.incl_len);
				if (!file.read(raw.data(), record.incl_len)) break;

				// Reconstruct the timestamp from the record header
				auto secs = std::chrono::seconds(record.ts_sec);
				auto usecs = std::chrono::microseconds(record.ts_usec);
				std::chrono::system_clock::time_point timestamp(
					std::chrono::duration_cast<std::chrono::system_clock::duration>(secs + usecs)
				);

				// Re-parse the IP header to reconstruct the packet fields
				if (raw.size() < sizeof(IP::IP_HDR)) continue;

				IP::IP_HDR* p_ip_header = reinterpret_cast<IP::IP_HDR*>(raw.data());
				BYTE next_header_offset = IP::get_header(*p_ip_header);
				BYTE protocol = p_ip_header->protocol;

				PACKET packet{};
				packet.raw = raw;
				packet.timestamp = timestamp;
				packet.source.addr.s_addr = p_ip_header->source_addr;
				packet.destination.addr.s_addr = p_ip_header->dest_addr;

				BYTE payload_offset = next_header_offset;
				WORD payload_length = 0;

				switch (protocol)
				{
				case IPPROTO_TCP:
					if (raw.size() <= next_header_offset + sizeof(IP::TCP)) continue;
					{
						IP::TCP* tcp = reinterpret_cast<IP::TCP*>(raw.data() + next_header_offset);
						WORD tcp_len = (tcp->offset_reserve >> 4) * 4;
						payload_offset = next_header_offset + tcp_len;
						packet.protocol = Packet::Protocol::TCP;
						packet.source.port = ntohs(tcp->source_port);
						packet.destination.port = ntohs(tcp->dest_port);
					}
					break;

				case IPPROTO_UDP:
					if (raw.size() <= next_header_offset + sizeof(IP::UDP)) continue;
					{
						IP::UDP* udp = reinterpret_cast<IP::UDP*>(raw.data() + next_header_offset);
						payload_offset = next_header_offset + sizeof(IP::UDP);
						packet.protocol = Packet::Protocol::UDP;
						packet.source.port = ntohs(udp->source_port);
						packet.destination.port = ntohs(udp->dest_port);
					}
					break;

				case IPPROTO_ICMP:
					packet.protocol = Packet::Protocol::ICMP;
					break;

				case IPPROTO_IGMP:
					packet.protocol = Packet::Protocol::IGMP;
					break;

				case IPPROTO_SCTP:
					if (raw.size() <= next_header_offset + 12) continue;
					payload_offset = next_header_offset + 12;
					packet.protocol = Packet::Protocol::SCTP;
					packet.source.port = ntohs(*reinterpret_cast<WORD*>(raw.data() + next_header_offset));
					packet.destination.port = ntohs(*reinterpret_cast<WORD*>(raw.data() + next_header_offset + 2));
					break;

				default:
					packet.protocol = Packet::Protocol::OTHER;
					break;
				}

				if ((int)raw.size() <= payload_offset) continue;
				payload_length = (WORD)(raw.size() - payload_offset);
				packet.offset = payload_offset;
				packet.length = payload_length;

				// Determine direction from the stored local IP!
				DWORD local_ip = Pawket::IP_UTIL::get_local_ip().s_addr;
				if (packet.destination.addr.s_addr == local_ip)
					packet.direction = Packet::Direction::INCOMING;
				else if (packet.source.addr.s_addr == local_ip)
					packet.direction = Packet::Direction::OUTGOING;

				{
					// Lock guarded mutex
					std::lock_guard<std::mutex> lock(Pawket::Packet::List::list_mutex);
					if (Pawket::Packet::List::packet_list.size() < (size_t)Config::config.MAX_PACKETS)
						Pawket::Packet::List::packet_list.push_back(packet);
				}
			}

			return true;
		}

	}
}

#endif