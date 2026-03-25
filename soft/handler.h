#ifndef _HANDLER
#define _HANDLER

#include <iostream>

#include <winsock2.h>

#include <mstcpip.h>

#include "util/ip_util.h"
#include "struct/ip_header.h"

#include <thread>
#include <vector>
#include <atomic>
#include <string>
#include <iomanip>
#include "util/packet.h"
#include "util/list.h"

using Pawket::IP::IP_HDR;

namespace Pawket
{
	namespace Handler
	{
		std::string get_packet_protocol_string(const PACKET& packet);
		void setup_capture_thread(); // Forward declare to use in initialize(); as I don't feel like moving it.

		inline std::atomic<bool> capture_running = true;
		inline std::thread capture_thread;

		class SocketHandler
		{
		private:
			SOCKET _socket;

		public:
			SocketHandler() {
				_socket = INVALID_SOCKET;
			};

			~SocketHandler() {
				close();
			};

			SOCKET get()
			{
				return _socket != INVALID_SOCKET ? _socket : INVALID_SOCKET;
			}

			void set(const SOCKET& socket)
			{
				_socket = socket;
			}

			void close()
			{
				if (_socket != INVALID_SOCKET)
					closesocket(_socket);
			}
		};

		// Create a new Socket Handler instance.
		inline SocketHandler socket_handler;

		bool initialize()
		{
			// Call WSAStartup with the needed params
			WSADATA wsa_data{};
			int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
			if (wsa_result != 0) return false;

			// Create a socket
			SOCKET _socket = socket(AF_INET, SOCK_RAW, IPPROTO_IP);
			if (_socket == INVALID_SOCKET) return false;

			// Update the handlers socket
			socket_handler.set(_socket);

			// Build sockaddr
			sockaddr_in socket_address_in {};
			socket_address_in.sin_addr = Pawket::IP_UTIL::get_local_ip();
			socket_address_in.sin_family = AF_INET;
			socket_address_in.sin_port = 0;

			// Bind the socket
			int bind_success = bind(socket_handler.get(), reinterpret_cast<sockaddr*>(&socket_address_in), sizeof(sockaddr_in));
			if (bind_success != 0) return false;

			// Setup WSAIoctl params
			DWORD rcvall_on = 1;
			DWORD bytes_received {};

			// Call WSAIoctl with proper parameters
			int ioctl_result = WSAIoctl(
				socket_handler.get(),
				SIO_RCVALL,
				&rcvall_on,
				sizeof(DWORD),
				nullptr,
				0,
				&bytes_received,
				nullptr,
				nullptr
			);

			if (ioctl_result != 0) return false;

			// Finally, set up the capture thread
			setup_capture_thread();

			return true;
		}

		void setup_capture_thread()
		{
			capture_thread = std::thread([]()
				{
					// Store the localhost for future use
					DWORD cached_local_ip = Pawket::IP_UTIL::get_local_ip().s_addr;

					std::vector<char> buffer{};
					buffer.resize(65535); // 1 char = 1 byte, so it's fine.
										  // vectors are slightly slower, but arrays are a headache.

					while (capture_running)
					{
						// Set up a 100ms timeout
						timeval timeout{};
						timeout.tv_sec = 0;
						timeout.tv_usec = 100000;

						// Set up the fd_set with our socket
						fd_set read_set{};
						FD_ZERO(&read_set);
						FD_SET(socket_handler.get(), &read_set);

						// Wait for data or timeout
						int ready = select(0, &read_set, nullptr, nullptr, &timeout);
						if (ready <= 0) continue; // Timeout or error, loop back and recheck capture_running

						int received = recv(socket_handler.get(), buffer.data(), buffer.size(), 0);
						if (received == SOCKET_ERROR) break; // Break out of the loop if our socket is invalid.

						// Read the raw bytes from recv for
						// 1. Version & header length.
						// 2. Protocol
						// 3. Source / destination
						// (Yes, I made the comment look like AI for the bit.)

						IP_HDR* p_ip_header = reinterpret_cast<IP_HDR*>(buffer.data());
						BYTE next_header_offset = IP::get_header(*p_ip_header);
						BYTE protocol = p_ip_header->protocol;

						BYTE payload_offset;
						WORD payload_length;

						PACKET packet{};

						IP::TCP* p_tcp_header{ nullptr };
						IP::UDP* p_udp_header{ nullptr };

						WORD tcp_header_length;

						// Fill in the non-specific fields.
						packet.source.addr.s_addr = p_ip_header->source_addr;
						packet.destination.addr.s_addr = p_ip_header->dest_addr;

						// Is the packet incoming or outgoing?
						if (packet.destination.addr.s_addr == cached_local_ip)
							packet.direction = Packet::Direction::INCOMING;

						else if (packet.source.addr.s_addr == cached_local_ip)
							packet.direction = Packet::Direction::OUTGOING;

						// Fill in the specific fields.
						switch (protocol)
						{
						case IPPROTO_TCP:
							// Packet Setup
							p_tcp_header = reinterpret_cast<IP::TCP*>(buffer.data() + next_header_offset);

							// Payload Setup
							tcp_header_length = (p_tcp_header->offset_reserve >> 4) * 4;
							payload_offset = next_header_offset + tcp_header_length;
							if (received <= payload_offset) continue; // Make sure the payload offset isn't zero

							packet.protocol = Packet::Protocol::TCP;
							packet.source.port = ntohs(p_tcp_header->source_port);
							packet.destination.port = ntohs(p_tcp_header->dest_port);

							payload_length = received - payload_offset; // Update the payload length
							packet.payload.assign(buffer.data() + payload_offset, buffer.data() + payload_offset + payload_length); // Populate the payload

							break;

							// The UDP setup is somewhat similar to the TCP, so the same comments apply.
							//		  If you're confused by anything here, look it up, as with anything.
						case IPPROTO_UDP:
							// Packet Setup
							p_udp_header = reinterpret_cast<IP::UDP*>(buffer.data() + next_header_offset);
							packet.protocol = Packet::Protocol::UDP;
							packet.source.port = ntohs(p_udp_header->source_port);
							packet.destination.port = ntohs(p_udp_header->dest_port);

							// Payload Setup
							payload_offset = next_header_offset + sizeof(IP::UDP);

							payload_length = received - payload_offset;

							if (received <= payload_offset) continue;

							packet.payload.assign(buffer.data() + payload_offset, buffer.data() + payload_offset + payload_length);

							break;

						case IPPROTO_ICMP:
							// No ports on ICMP. Payload is the entire ICMP message (header + data).
							payload_offset = next_header_offset;
							if (received <= payload_offset) continue;

							packet.protocol = Packet::Protocol::ICMP;
							packet.source.port = 0;
							packet.destination.port = 0;
							payload_length = received - payload_offset;
							packet.payload.assign(buffer.data() + payload_offset, buffer.data() + payload_offset + payload_length);
							break;

						case IPPROTO_IGMP:
							// Same as ICMP — no ports, grab from the IP header boundary.
							payload_offset = next_header_offset;
							if (received <= payload_offset) continue;

							packet.protocol = Packet::Protocol::IGMP;
							packet.source.port = 0;
							packet.destination.port = 0;
							payload_length = received - payload_offset;
							packet.payload.assign(buffer.data() + payload_offset, buffer.data() + payload_offset + payload_length);
							break;

						case IPPROTO_SCTP:
							// SCTP header: src port (2) + dst port (2) + vtag (4) + checksum (4) = 12 bytes.
							payload_offset = next_header_offset + 12;
							if (received <= payload_offset) continue;

							packet.protocol = Packet::Protocol::SCTP;
							packet.source.port = ntohs(*reinterpret_cast<WORD*>(buffer.data() + next_header_offset));
							packet.destination.port = ntohs(*reinterpret_cast<WORD*>(buffer.data() + next_header_offset + 2));
							payload_length = received - payload_offset;
							packet.payload.assign(buffer.data() + payload_offset, buffer.data() + payload_offset + payload_length);
							break;

						default:
							// Capture anything else as OTHER
							payload_offset = next_header_offset;
							if (received <= payload_offset) continue;

							packet.protocol = Packet::Protocol::OTHER;
							packet.source.port = 0;
							packet.destination.port = 0;
							payload_length = received - payload_offset;
							packet.payload.assign(buffer.data() + payload_offset, buffer.data() + payload_offset + payload_length);
							break;
						}

						// Check if the packet should be printed.
						if (Config::config.filter != Config::FilterType::ANY)
						{
							switch (packet.direction)
							{
							case Packet::Direction::INCOMING:
								if (!(Config::config.filter == Config::FilterType::INCOMING))
									continue;
								break;
							case Packet::Direction::OUTGOING:
								if (!(Config::config.filter == Config::FilterType::OUTGOING))
									continue;
								break;
							}
						}

						{
							// Lock guarded mutex
							std::lock_guard<std::mutex> lock(Pawket::Packet::List::list_mutex);
							if (Pawket::Packet::List::packet_list.size() < Config::config.MAX_PACKETS)
								Pawket::Packet::List::packet_list.push_back(packet);
						}

						if (Config::config.debug)
						{
							// Print packet info
							char source_str[INET_ADDRSTRLEN];
							char dest_str[INET_ADDRSTRLEN];

							inet_ntop(AF_INET, &packet.source.addr, source_str, INET_ADDRSTRLEN);
							inet_ntop(AF_INET, &packet.destination.addr, dest_str, INET_ADDRSTRLEN);

							std::cout << "[+] Packet Captured!\n";
							std::cout << "[+] ================\n";
							std::cout << "[+] Source: " << source_str << " : " << std::to_string(packet.source.port) << "\n";
							std::cout << "[+] Dest: " << dest_str << " : " << std::to_string(packet.destination.port) << "\n";
							std::cout << "[+] Protocol: " << get_packet_protocol_string(packet) << "\n";

							// Hex dump
							char ascii[17]{};

							std::cout << "[+] Payload:\n";
							for (size_t i = 0; i < packet.payload.size(); i++)
							{
								std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)(BYTE)packet.payload[i] << " ";
								ascii[i % 16] = std::isprint((unsigned char)packet.payload[i]) ? packet.payload[i] : '.';

								if ((i + 1) % 16 == 0)
								{
									std::cout << " | " << ascii << "\n";
								}
							}

							if (packet.payload.size() % 16 != 0)
							{
								size_t remaining = packet.payload.size() % 16;
								std::cout << std::string((16 - remaining) * 3, ' ');
								ascii[remaining] = '\0';
								std::cout << " | " << ascii << "\n";
							}

							std::cout << std::dec << "\n";
							std::cout << "[+] ================\n";
						}
					}
				}
			);

			if(Config::config.debug)
				std::cout << "[+] Setup packet capture thread.\n";
			
			return;
		}

		// Helper function to get the string value of the packets protocol.
		std::string get_packet_protocol_string(const PACKET& packet)
		{
			switch (packet.protocol)
			{
			case Packet::Protocol::UDP:
				return "UDP";
			case Packet::Protocol::TCP:
				return "TCP";
			case Packet::Protocol::ICMP:
				return "ICMP";
			case Packet::Protocol::IGMP:
				return "IGMP";
			case Packet::Protocol::SCTP:
				return "SCTP";
			case Packet::Protocol::OTHER:
				return "OTHER";
			default:
				return "UNKNOWN";
			}
		}
	}
}

#endif