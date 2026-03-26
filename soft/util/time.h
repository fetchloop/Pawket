#ifndef _TIME_H
#define _TIME_H

#include <iostream>

#include <chrono>
#include <string>
#include <ctime>

#include "config.h"

namespace Pawket
{
	namespace Time
	{
		// Returns a string in the format HH::MM::SS..mmm
		std::string get_string_timestamp(const std::chrono::system_clock::time_point& timepoint)
		{
			struct tm nt;
			time_t time = std::chrono::system_clock::to_time_t(timepoint);
			if (localtime_s(&nt, &time) == 0)
			{
				char buf[16];
				size_t strf = strftime(buf, sizeof(buf), "%H:%M:%S", &nt);
				snprintf(buf + strf, sizeof(buf) - strf, ".%03d",
					duration_cast<std::chrono::milliseconds>(timepoint.time_since_epoch()).count() % 1000
				);
				return std::string(buf);
			}
			else
				if (Pawket::Config::config.debug)
					std::cout << "[-] Failed to convert timestamp of timepoint '" << time << "'.\n";

			return "";
		}

		// Returns a filename compatible timestamp string in the format YYYY-MM-DD_HH-MM-SS
		std::string get_win_string_timestamp(const std::chrono::system_clock::time_point& timepoint)
		{
			struct tm nt;
			time_t time = std::chrono::system_clock::to_time_t(timepoint);
			if (localtime_s(&nt, &time) == 0)
			{
				char buf[20];
				strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &nt);
				return std::string(buf);
			}
			else
				if (Pawket::Config::config.debug)
					std::cout << "[-] Failed to convert timestamp of timepoint '" << time << "'.\n";

			return "unknown";
		}
	}
}

#endif