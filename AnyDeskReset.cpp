#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

namespace fs = std::filesystem;
#define thread_sleep(x) std::this_thread::sleep_for( std::chrono::milliseconds(x) )

bool kill_process(const wchar_t* processName) {
    HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);
    PROCESSENTRY32 pEntry;
    pEntry.dwSize = sizeof(pEntry);
    BOOL hRes = Process32First(hSnapShot, &pEntry);
    while (hRes) {
        if (wcscmp(pEntry.szExeFile, processName) == 0) {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, 0, (DWORD)pEntry.th32ProcessID);
            if (hProcess != NULL) {
                TerminateProcess(hProcess, 9);
                CloseHandle(hProcess);
            }
        }
        hRes = Process32Next(hSnapShot, &pEntry);
    }
    CloseHandle(hSnapShot);
    return true;
}

bool start_process_as_admin(const wchar_t* processPath) {
    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.lpVerb = L"runas";
    sei.lpFile = processPath;
    sei.nShow = SW_NORMAL;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (!ShellExecuteExW(&sei)) {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            std::wcerr << L" > The user refused to allow privileges elevation." << std::endl;
        }
        else {
            std::wcerr << L" > ShellExecuteEx failed, error: " << error << std::endl;
        }
        return false;
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 0);
        CloseHandle(sei.hProcess);
    }

    return true;
}

bool start_process(const wchar_t* processPath) {

	if (!start_process_as_admin(processPath)) {
		std::wcerr << L" > Failed to start process as admin, trying to start as normal user..." << std::endl;
	}

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessW(processPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }

	std::wcerr << L" > Failed to start process: " << processPath << std::endl;
    return false;
}

std::wstring get_env_variable(const wchar_t* name) {
    wchar_t buffer[MAX_PATH];
    DWORD result = GetEnvironmentVariableW(name, buffer, MAX_PATH);
    if (result == 0) {
        std::wcerr << L" > Failed to get environment variable: " << name << std::endl;
        return L"";
    }
    return std::wstring(buffer);
}

std::wstring get_config_parameter(const std::wstring& configPath, const std::wstring& parameter) {
    try
    {
        std::wifstream file(configPath);
        std::wstring line;
        while (std::getline(file, line)) {
            if (line.find(parameter) != std::wstring::npos) {
                return line.substr(line.find(L"=") + 1);
            }
        }
    }
	catch (const std::exception& e)
	{
		std::wcerr << L" > Error reading config file: " << e.what() << std::endl;
	}
	return L"";
}

bool set_config_parameter(const std::wstring& configPath, const std::wstring& parameter, const std::wstring& value) {

	// Read the entire file into memory
	std::wstring fileContent;
	try
	{
		std::wifstream file(configPath);
		fileContent.assign((std::istreambuf_iterator<wchar_t>(file)),
			std::istreambuf_iterator<wchar_t>());
	}
	catch (const std::exception& e)
	{
		std::wcerr << L" > Error reading config file: " << e.what() << std::endl;
		return false;
	}

	// Find if parameter exists
	size_t pos = fileContent.find(parameter);
	if (pos != std::wstring::npos) {
		// Replace the existing value
		size_t endPos = fileContent.find(L"\n", pos);
		fileContent.replace(pos, endPos - pos, parameter + value);
	}
	else {
		// Append the new parameter
		fileContent += parameter + value + L"\n";
	}

	// Write the modified content back to the file
	try
	{
		std::wofstream file(configPath);
		file << fileContent;
	}
	catch (const std::exception& e)
	{
		std::wcerr << L" > Error writing config file: " << e.what() << std::endl;
		return false;
	}

	return true;
}

bool reset_anydesk() {

    /*
    
    C:\Users\Username\AppData\Roaming\AnyDesk\

    delete everything except user.png
    
    */

	// Check if file path exists
	std::wstring userProfile = get_env_variable(L"USERPROFILE");
	std::wstring anydeskRoaming = userProfile + L"\\AppData\\Roaming\\AnyDesk\\";

	if (!fs::exists(anydeskRoaming)) {
		return true;
	}

	for (const auto& entry : fs::directory_iterator(anydeskRoaming)) {
		if (entry.is_regular_file() && entry.path().filename() != L"user.png") {
            if (!fs::remove(entry.path())) {
				std::cout << " > Failed to delete: " << entry.path() << std::endl;
            }
			else {
				std::cout << " > Deleted: " << entry.path() << std::endl;
			}
		}
        // delete any folder
		if (entry.is_directory()) {
			if (!fs::remove_all(entry.path())) {
				std::cout << " > Failed to delete folder: " << entry.path() << std::endl;
			}
			else {
				std::cout << " > Deleted folder: " << entry.path() << std::endl;
			}
		}
	}

	return true;
}

int main() {
	// https://github.com/suspex0/anydesk-reset-identity
	// Developer: suspex0
	// Miauuu <3
	SetConsoleTitleW(L"AnyDesk Reset Tool");

	std::wcout << L" > Please always start this application as administrator!" << std::endl << std::endl;

    std::wcout << L" > Killing AnyDesk process..." << std::endl;
	kill_process(L"AnyDesk.exe");
    thread_sleep(1000);

	// C:\\Users\\<username>\\AppData\\Roaming\\AnyDesk\\user.conf
	std::wstring userProfile = get_env_variable(L"USERPROFILE");
	std::wstring configPath = userProfile + L"\\AppData\\Roaming\\AnyDesk\\user.conf";

	std::wstring username = get_config_parameter(configPath, L"ad.privacy.name=");
    std::wstring show_username = L"ad.privacy.name.show=";

	std::wcout << L" > Username detected: " << username << std::endl;

	// Reset AnyDesk
    if (!reset_anydesk()) {
		std::wcerr << L" > Failed to reset AnyDesk." << std::endl;
		system("pause");
		return 1;
    }

    thread_sleep(1000);

	if (!username.empty()) {
		// Update username
		if (!set_config_parameter(configPath, L"ad.privacy.name=", username)) {
			std::wcerr << L" > Failed to set username in config." << std::endl;
		}

		// Update show username
		if (!set_config_parameter(configPath, show_username, L"2")) {
			std::wcerr << L" > Failed to set show username in config." << std::endl;
		}
	}

	// If there is user.png in the folder
	if (fs::exists(userProfile + L"\\AppData\\Roaming\\AnyDesk\\user.png")) {
		if (!set_config_parameter(configPath, L"ad.privacy.image.show=", L"2")) {
			std::wcerr << L" > Failed to set username in config." << std::endl;
		}
	}

	thread_sleep(1000);

	// Start AnyDesk
	std::wcout << L" > Starting AnyDesk..." << std::endl;
    if (!start_process(L"C:\\Program Files (x86)\\AnyDesk\\AnyDesk.exe")) {
        std::wcerr << L" > Failed to start AnyDesk." << std::endl;
        system("pause");
        return 1;
    }

    thread_sleep(1000);

	std::cout << " > Recovering configuration..." << std::endl;

	bool username_set = false, image_set = false;

    while (true) {

		if (!username.empty()) {
			// Update username
			if (!set_config_parameter(configPath, L"ad.privacy.name=", username)) {
				std::wcerr << L" > Failed to set username in config." << std::endl;
			}
			else
			{
				std::wcout << L" > Username set to: " << username << std::endl;
			}
			// Update show username
			if (!set_config_parameter(configPath, show_username, L"2")) {
				std::wcerr << L" > Failed to set show username in config." << std::endl;
			}
			else
			{
				std::wcout << L" > Show username set to: 2" << std::endl;
			}

			thread_sleep(1000);

			if (get_config_parameter(configPath, L"ad.privacy.name=") == username) {
				username_set = true;
			}
		}
		else {
			std::cout << " > No username found, skipping username setting." << std::endl;
			username_set = true;
		}
		
		// If there is user.png in the folder
		if (fs::exists(userProfile + L"\\AppData\\Roaming\\AnyDesk\\user.png")) {
			if (!set_config_parameter(configPath, L"ad.privacy.image.show=", L"2")) {
				std::wcerr << L" > Failed to set username in config." << std::endl;
			}
			else
			{
				std::wcout << L" > User image enabled." << std::endl;
			}

			thread_sleep(1000);

			if (get_config_parameter(configPath, L"ad.privacy.image.show=") == L"2") {
				image_set = true;
			}
		}
		else
		{
			std::cout << " > user.png not found, skipping image setting." << std::endl;
			image_set = true;
		}

		thread_sleep(1000);

		if (username_set && image_set) {
			break;
		}
    }

	std::wcout << L" > AnyDesk reset completed, exit in 5 seconds..." << std::endl;
	thread_sleep(5000);
    return 0;
}
