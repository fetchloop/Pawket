#include <iostream>

#include "util/config.h"

#include "handler.h"

#include "gui/gui.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace Pawket;

// Forward declare the software rights function.
BOOL is_elevated();

int main()
{
	// First try to load the config.
	// If it doesn't exist, create a new one.
	if (!Config::load(Config::get_config_path()))
		Config::create();

	// Check program elevation
	if (!is_elevated())
	{
		MessageBoxA(nullptr, "Pawket requires administrator permissions to run.", "Pawket", MB_OK);
		return 1;
	}

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

BOOL is_elevated()
{
	BOOL f_ret = FALSE;
	HANDLE h_token = NULL;

	if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h_token))
	{
		TOKEN_ELEVATION elevation;
		DWORD cb_size = sizeof(TOKEN_ELEVATION);
		if (GetTokenInformation(h_token, TokenElevation, &elevation, sizeof(elevation), &cb_size))
			f_ret = elevation.TokenIsElevated;
	}

	if (h_token)
		CloseHandle(h_token);

	return f_ret;
}