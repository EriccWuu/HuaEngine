#include "enginepch.h"
#include "HostLaunch.h"

#include <system_error>

#include "HuaEngine/Core/ResourcePaths.h"

#ifdef HE_PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace {
#ifdef HE_PLATFORM_WINDOWS
	std::wstring QuoteArgument(const std::wstring& value) {
		if (value.empty()) {
			return L"\"\"";
		}

		const bool needsQuotes = value.find_first_of(L" \t\"") != std::wstring::npos;
		if (!needsQuotes) {
			return value;
		}

		std::wstring quoted = L"\"";
		for (wchar_t character : value) {
			if (character == L'"') {
				quoted += L'\\';
			}
			quoted += character;
		}
		quoted += L"\"";
		return quoted;
	}

	std::wstring BuildCommandLine(const std::filesystem::path& executablePath, const std::vector<std::string>& arguments) {
		std::wstring commandLine = QuoteArgument(executablePath.wstring());
		for (const auto& argument : arguments) {
			commandLine += L' ';
			commandLine += QuoteArgument(std::filesystem::path(argument).wstring());
		}
		return commandLine;
	}
#endif
}

namespace HE {
	std::filesystem::path HostLaunch::ResolveSiblingExecutable(const std::filesystem::path& executableName) {
		std::error_code errorCode;
		const auto candidate = ResourcePaths::GetExecutableDirectory() / executableName;
		if (std::filesystem::exists(candidate, errorCode)) {
			return std::filesystem::weakly_canonical(candidate, errorCode);
		}

		return candidate.lexically_normal();
	}

	bool HostLaunch::Launch(const std::filesystem::path& executablePath, const std::vector<std::string>& arguments, const std::filesystem::path& workingDirectory) {
#ifdef HE_PLATFORM_WINDOWS
		std::error_code errorCode;
		if (!std::filesystem::exists(executablePath, errorCode) || !std::filesystem::is_regular_file(executablePath, errorCode)) {
			return false;
		}

		auto commandLine = BuildCommandLine(executablePath, arguments);
		std::wstring mutableCommandLine = commandLine;
		auto workingDirectoryWide = workingDirectory.empty() ? std::wstring() : std::filesystem::path(workingDirectory).wstring();

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInformation{};

		const BOOL created = CreateProcessW(
			executablePath.wstring().c_str(),
			mutableCommandLine.data(),
			nullptr,
			nullptr,
			FALSE,
			0,
			nullptr,
			workingDirectoryWide.empty() ? nullptr : workingDirectoryWide.c_str(),
			&startupInfo,
			&processInformation);

		if (!created) {
			return false;
		}

		CloseHandle(processInformation.hThread);
		CloseHandle(processInformation.hProcess);
		return true;
#else
		(void)executablePath;
		(void)arguments;
		(void)workingDirectory;
		return false;
#endif
	}

	bool HostLaunch::LaunchSibling(const std::filesystem::path& executableName, const std::vector<std::string>& arguments, const std::filesystem::path& workingDirectory) {
		return Launch(ResolveSiblingExecutable(executableName), arguments, workingDirectory);
	}
}
