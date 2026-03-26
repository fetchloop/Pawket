#ifndef _CONFIG
#define _CONFIG

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>

namespace Pawket
{
	namespace Config
	{
		enum FilterType
		{
			ANY,
			INCOMING,
			OUTGOING,
		};

		std::filesystem::path get_config_path();
		std::string get_filter_string_from_filter(FilterType);

		struct Config
		{
			FilterType filter;
			int MAX_PACKETS = 100;
			bool debug{};
			bool dark_mode = true;
		};

		inline Config config;

		bool load(std::filesystem::path path)
		{
			if (!std::filesystem::exists(path)) return false;

			std::ifstream file(path);
			if (file.is_open())
			{
				std::string line{};

				while (std::getline(file, line))
				{
					size_t pos = line.find('=');
					if (pos == std::string::npos) continue;

					std::string key, value;
					key = line.substr(0, pos);
					value = line.substr(pos + 1);

					if (key == "filter")
					{
						if (value == "INCOMING") config.filter = FilterType::INCOMING;
						else if (value == "OUTGOING") config.filter = FilterType::OUTGOING;
						else config.filter = FilterType::ANY;
					}
					else if (key == "MAX_PACKETS")
					{
						try
						{
							config.MAX_PACKETS = std::stoi(value);
						}
						catch (const std::exception&) {};
					}
					else if (key == "debug")
					{
						config.debug = value == "true";
					}
					else if (key == "dark_mode")
					{
						config.dark_mode = value == "true";
					}
				}

				file.close();
			}

			return true;
		};

		void create()
		{
			std::filesystem::path config_path = get_config_path();

			// Create the actual config folder
			std::filesystem::create_directories(config_path.parent_path());

			// Create and open the config file
			std::ofstream file(config_path);

			// Write the key values
			file << "filter=" << get_filter_string_from_filter(config.filter) << "\n";
			file << "MAX_PACKETS=" << config.MAX_PACKETS << "\n";
			file << "debug=" << (config.debug ? "true" : "false") << "\n";
			file << "dark_mode=" << (config.dark_mode ? "true" : "false") << "\n";

			if (config.debug)
				std::cout << "[+] Successfully created config.\n";
		}; // Save the current config into a file on the disk.

		bool save()
		{
			std::filesystem::path config_path = get_config_path();

			// Check if the config file exists.
			if (!std::filesystem::exists(config_path))
				return false;

			create();
			return true;
		};

		std::string get_filter_string_from_filter(FilterType filter_type)
		{
			switch (filter_type)
			{
			case FilterType::ANY:
				return "ANY";
			case FilterType::INCOMING:
				return "INCOMING";
			case FilterType::OUTGOING:
				return "OUTGOING";
			default:
				return "ANY";
			}
		}

		std::filesystem::path get_config_path()
		{
			wchar_t appdata_path[MAX_PATH];
			SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, appdata_path);

			std::filesystem::path config_path = std::filesystem::path(appdata_path) / "Pawket" / "config.cfg";
			return config_path;
		}
	}
}
#endif