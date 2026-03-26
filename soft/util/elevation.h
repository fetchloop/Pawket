#ifndef _ELEVATION_H
#define _ELEVATION_H

#include <Windows.h>
#include <shellapi.h>

namespace Pawket
{
	namespace Elevation
	{
		BOOL is_elevated()
		{
			BOOL f_ret = FALSE;
			HANDLE h_token = NULL;

			if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h_token))
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

		// Returns true if a relaunch was triggered (caller should exit)
		bool request_elevation()
		{
			if (is_elevated()) return false; // already elevated

			wchar_t path[MAX_PATH];
			GetModuleFileNameW(nullptr, path, MAX_PATH);

			SHELLEXECUTEINFOW sei = {};
			sei.cbSize = sizeof(sei);
			sei.lpVerb = L"runas";
			sei.lpFile = path;
			sei.nShow = SW_SHOWNORMAL;

			if (!ShellExecuteExW(&sei))
				MessageBoxA(nullptr, "Pawket requires administrator permissions to run.", "Pawket", MB_OK);

			return true; // either relaunched or failed, exit either way.
		}
	}
}

#endif