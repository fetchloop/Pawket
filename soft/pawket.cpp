#include <iostream>

#include "util/config.h"
#include "handler.h"
#include "gui/gui.h"
#include "util/elevation.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace Pawket;

// Forward declare the software rights function.
BOOL is_elevated();

int main()
{
	// If a relaunch was triggered (or UAC was declined), exit this instance.
	if (Pawket::Elevation::request_elevation())
		return 0;

	// First try to load the config.
	// If it doesn't exist, create a new one.
	if (!Config::load(Config::get_config_path()))
		Config::create();

	if (!Handler::initialize())
	{
		MessageBoxA(nullptr, "Something went wrong during socket setup. Please try again.", "Pawket", MB_OK);
		return 1;
	}

	// Run the GUI loop after setting up the capture thread.
	Pawket::GUI::render_loop();

	// Cleanup
	Handler::capture_running = false;
	if (Handler::capture_thread.joinable())
		Handler::capture_thread.join();

	Handler::socket_handler.close();

	Config::save();

	WSACleanup();
	return 0;
}