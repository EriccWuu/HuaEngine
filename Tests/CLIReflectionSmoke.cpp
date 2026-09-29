#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {
	std::vector<std::filesystem::path>& CleanupPaths() {
		static std::vector<std::filesystem::path> paths;
		return paths;
	}

	void CleanupRegisteredPaths() {
		std::error_code errorCode;
		for (const auto& path : CleanupPaths()) {
			std::filesystem::remove_all(path, errorCode);
			errorCode.clear();
		}
	}

	void RegisterCleanupPath(std::filesystem::path path) {
		CleanupPaths().push_back(std::move(path));
	}

	struct ProcessResult {
		DWORD ExitCode = 0;
		std::string Output;
	};

	void Expect(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[CLIReflectionSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	std::wstring Utf8ToWide(std::string_view value) {
		if (value.empty()) {
			return {};
		}

		const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
		Expect(required > 0, "Failed to convert UTF-8 text to UTF-16");

		std::wstring wide(required, L'\0');
		const int written = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), required);
		Expect(written == required, "Failed to complete UTF-8 to UTF-16 conversion");
		return wide;
	}

	std::wstring QuoteForCommandLine(const std::wstring& argument) {
		if (argument.empty()) {
			return L"\"\"";
		}

		if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
			return argument;
		}

		std::wstring quoted = L"\"";
		size_t backslashCount = 0;
		for (const wchar_t character : argument) {
			if (character == L'\\') {
				++backslashCount;
				continue;
			}

			if (character == L'"') {
				quoted.append(backslashCount * 2 + 1, L'\\');
				quoted += L'"';
				backslashCount = 0;
				continue;
			}

			if (backslashCount > 0) {
				quoted.append(backslashCount, L'\\');
				backslashCount = 0;
			}

			quoted += character;
		}

		if (backslashCount > 0) {
			quoted.append(backslashCount * 2, L'\\');
		}

		quoted += L"\"";
		return quoted;
	}

	std::wstring BuildCommandLine(const std::filesystem::path& executable, const std::vector<std::string>& arguments) {
		std::wstring commandLine = QuoteForCommandLine(executable.wstring());
		for (const auto& argument : arguments) {
			commandLine += L" ";
			commandLine += QuoteForCommandLine(Utf8ToWide(argument));
		}

		return commandLine;
	}

	ProcessResult RunCLICommand(
		const std::filesystem::path& executable,
		const std::vector<std::string>& arguments,
		const std::filesystem::path& workingDirectory) {
		SECURITY_ATTRIBUTES securityAttributes{};
		securityAttributes.nLength = sizeof(securityAttributes);
		securityAttributes.bInheritHandle = TRUE;

		HANDLE readPipe = nullptr;
		HANDLE writePipe = nullptr;
		Expect(CreatePipe(&readPipe, &writePipe, &securityAttributes, 0), "Failed to create output pipe");
		Expect(SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0), "Failed to mark read pipe as non-inheritable");

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		startupInfo.dwFlags = STARTF_USESTDHANDLES;
		startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		startupInfo.hStdOutput = writePipe;
		startupInfo.hStdError = writePipe;

		PROCESS_INFORMATION processInfo{};
		std::wstring commandLine = BuildCommandLine(executable, arguments);
		std::wstring workingDirectoryWide = workingDirectory.wstring();

		const BOOL created = CreateProcessW(
			executable.wstring().c_str(),
			commandLine.data(),
			nullptr,
			nullptr,
			TRUE,
			0,
			nullptr,
			workingDirectoryWide.c_str(),
			&startupInfo,
			&processInfo);
		CloseHandle(writePipe);
		Expect(created == TRUE, "Failed to launch HuaEngineCLI.exe");

		std::string output;
		char buffer[4096];
		DWORD bytesRead = 0;
		while (ReadFile(readPipe, buffer, static_cast<DWORD>(sizeof(buffer)), &bytesRead, nullptr) && bytesRead > 0) {
			output.append(buffer, bytesRead);
		}

		CloseHandle(readPipe);
		WaitForSingleObject(processInfo.hProcess, INFINITE);

		DWORD exitCode = 0;
		Expect(GetExitCodeProcess(processInfo.hProcess, &exitCode) == TRUE, "Failed to query process exit code");
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);

		return { exitCode, std::move(output) };
	}

	void ExpectContains(std::string_view output, std::string_view fragment, std::string_view context) {
		Expect(
			output.find(fragment) != std::string_view::npos,
			std::string(context) + ": missing fragment " + std::string(fragment) + "\n" + std::string(output));
	}

	void ExpectSuccess(const ProcessResult& result, std::string_view operation) {
		Expect(result.ExitCode == 0, std::string(operation) + " should succeed\n" + result.Output);
		ExpectContains(result.Output, "\"operation\":\"" + std::string(operation) + "\"", operation);
		ExpectContains(result.Output, "\"status\":\"success\"", operation);
		ExpectContains(result.Output, "\"reflected_type_count\"", operation);
		ExpectContains(result.Output, "\"reflected_enum_count\"", operation);
	}

	std::filesystem::path GetCurrentExecutablePath() {
		std::wstring buffer(MAX_PATH, L'\0');
		for (;;) {
			const DWORD required = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
			Expect(required != 0, "Failed to resolve current executable path");

			if (required < buffer.size() - 1) {
				buffer.resize(required);
				return std::filesystem::path(buffer);
			}

			buffer.resize(buffer.size() * 2);
		}
	}

	std::string ReadText(const std::filesystem::path& path) {
		std::ifstream input(path, std::ios::binary);
		Expect(input.is_open(), "Failed to read " + path.string());
		return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	}

	std::string Utf8PathArgument(const std::filesystem::path& path) {
		const auto encoded = path.generic_u8string();
		return { reinterpret_cast<const char*>(encoded.data()), encoded.size() };
	}

	std::filesystem::path ConfiguredPath(const std::string& config, std::string_view key) {
		const std::string quotedKey = "\"" + std::string(key) + "\"";
		const size_t keyAt = config.find(quotedKey);
		Expect(keyAt != std::string::npos, "Missing meta configuration key: " + std::string(key));
		const size_t separator = config.find(':', keyAt + quotedKey.size());
		Expect(separator != std::string::npos, "Invalid meta configuration key: " + std::string(key));
		const size_t valueAt = config.find('"', separator);
		Expect(valueAt != std::string::npos, "Invalid meta configuration value: " + std::string(key));
		std::string value;
		for (size_t index = valueAt + 1; index < config.size(); ++index) {
			const char character = config[index];
			if (character == '"') {
				const std::filesystem::path path = std::filesystem::u8path(value);
				Expect(path.is_absolute(), "Meta configuration path must be absolute: " + std::string(key));
				return path;
			}
			if (character == '\\' && index + 1 < config.size()) {
				const char escaped = config[++index];
				Expect(escaped == '\\' || escaped == '/' || escaped == '"', "Unsupported path escape in meta configuration");
				value += escaped;
			} else {
				value += character;
			}
		}
		Expect(false, "Unterminated meta configuration path: " + std::string(key));
		return {};
	}

	struct FileFingerprint {
		std::filesystem::file_time_type Modified;
		uint64_t Hash = 14695981039346656037ull;
		uintmax_t Size = 0;
	};

	FileFingerprint Fingerprint(const std::filesystem::path& path) {
		Expect(std::filesystem::is_regular_file(path), "Missing generated artifact: " + path.string());
		FileFingerprint result;
		result.Modified = std::filesystem::last_write_time(path);
		result.Size = std::filesystem::file_size(path);
		for (const unsigned char byte : ReadText(path)) {
			result.Hash = (result.Hash ^ byte) * 1099511628211ull;
		}
		return result;
	}

	void ValidateWithoutWriting(
		const std::filesystem::path& cliExecutable,
		const std::filesystem::path& workingDirectory,
		const std::filesystem::path& configPath,
		const std::string& config) {
		const auto manifest = ConfiguredPath(config, "manifest");
		const auto output = ConfiguredPath(config, "output_dir");
		const std::array<std::filesystem::path, 6> artifacts{
			manifest,
			output / "GeneratedReflection.h",
			output / "GeneratedReflection.cpp",
			output / "GeneratedEcs.h",
			output / "GeneratedEcs.cpp",
			output / "generation-stamp.json"
		};
		std::array<FileFingerprint, 6> before;
		for (size_t index = 0; index < artifacts.size(); ++index) {
			before[index] = Fingerprint(artifacts[index]);
		}

		ExpectSuccess(RunCLICommand(cliExecutable,
			{ "reflection", "validate", "--meta-config", Utf8PathArgument(configPath) }, workingDirectory), "reflection.validate");
		for (size_t index = 0; index < artifacts.size(); ++index) {
			const auto after = Fingerprint(artifacts[index]);
			Expect(after.Modified == before[index].Modified && after.Size == before[index].Size && after.Hash == before[index].Hash,
				"Validation must not rewrite " + artifacts[index].string());
		}
	}
}

int main(int argc, char** argv) {
	(void)CleanupPaths();
	std::atexit(CleanupRegisteredPaths);
	Expect(argc == 2, "CTest must pass the absolute Core meta-config path");
	const std::filesystem::path configPath = std::filesystem::absolute(std::filesystem::u8path(argv[1]));
	Expect(std::filesystem::is_regular_file(configPath), "Missing CMake-generated meta configuration");
	const std::string config = ReadText(configPath);

	const auto cliExecutable = GetCurrentExecutablePath().parent_path() / "HuaEngineCLI.exe";
	Expect(std::filesystem::is_regular_file(cliExecutable), "HuaEngineCLI.exe must exist next to the smoke executable");
	const auto workingDirectory = std::filesystem::current_path();
	const auto missingConfig = RunCLICommand(cliExecutable, { "reflection", "scan" }, workingDirectory);
	Expect(missingConfig.ExitCode != 0, "Scanning without --meta-config must fail");
	ExpectContains(missingConfig.Output, "--meta-config", "missing meta configuration");

	const auto fixtureWorkspace = configPath.parent_path() / ("cli-reflection-smoke-" + std::to_string(GetCurrentProcessId()));
	RegisterCleanupPath(fixtureWorkspace);
	const auto fixtureRoot = fixtureWorkspace / std::filesystem::u8path(u8"root with spaces %USERNAME% & unicode 空间");
	std::filesystem::create_directories(fixtureRoot);
	const auto fixtureConfig = fixtureRoot / "meta-config.json";
	std::filesystem::copy_file(configPath, fixtureConfig, std::filesystem::copy_options::overwrite_existing);
	const auto fixtureManifest = fixtureRoot / "manifest.json";
	const auto fixtureGenerated = fixtureRoot / "generated";

	ExpectSuccess(RunCLICommand(cliExecutable,
		{ "reflection", "scan", "--meta-config", Utf8PathArgument(fixtureConfig), "--out", Utf8PathArgument(fixtureManifest) },
		workingDirectory), "reflection.scan");
	Expect(std::filesystem::is_regular_file(fixtureManifest), "CLI scan must write the requested manifest");
	ExpectSuccess(RunCLICommand(cliExecutable,
		{ "reflection", "generate", "--meta-config", Utf8PathArgument(fixtureConfig), "--out", Utf8PathArgument(fixtureManifest),
			"--out-dir", Utf8PathArgument(fixtureGenerated) }, workingDirectory), "reflection.generate");
	Expect(std::filesystem::is_regular_file(fixtureGenerated / "GeneratedReflection.cpp"), "CLI must generate reflection source");
	Expect(std::filesystem::is_regular_file(fixtureGenerated / "GeneratedReflection.h"), "CLI must generate reflection declarations");
	Expect(std::filesystem::is_regular_file(fixtureGenerated / "GeneratedEcs.cpp"), "CLI must generate ECS registration source");
	Expect(std::filesystem::is_regular_file(fixtureGenerated / "GeneratedEcs.h"), "CLI must generate ECS component traits");

	ValidateWithoutWriting(cliExecutable, workingDirectory, configPath, config);

	std::cout << "CLIReflectionSmoke passed" << std::endl;
	return 0;
}
