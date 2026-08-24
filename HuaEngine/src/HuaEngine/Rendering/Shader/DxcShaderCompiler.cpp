#include "enginepch.h"
#include "DxcShaderCompiler.h"

#include <atomic>
#include <fstream>
#include <sstream>

#include "HuaEngine/Core/ResourcePaths.h"

#ifdef HE_PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace {
	constexpr DWORD CompileTimeoutMilliseconds = 30'000;
	std::atomic_uint64_t NextJobId = 1;
	class ScopedJobDirectory final {
	public:
		explicit ScopedJobDirectory(std::filesystem::path path) : m_Path(std::move(path)) {}
		~ScopedJobDirectory() { std::error_code errorCode; std::filesystem::remove_all(m_Path, errorCode); }
	private:
		std::filesystem::path m_Path;
	};

#ifdef HE_PLATFORM_WINDOWS
	std::wstring QuoteArgument(const std::wstring& value) {
		std::wstring quoted = L"\"";
		size_t backslashes = 0;
		for (const wchar_t character : value) {
			if (character == L'\\') { ++backslashes; continue; }
			if (character == L'\"') quoted.append(backslashes * 2 + 1, L'\\');
			else quoted.append(backslashes, L'\\');
			backslashes = 0;
			quoted.push_back(character);
		}
		quoted.append(backslashes * 2, L'\\');
		quoted.push_back(L'\"');
		return quoted;
	}

	std::wstring MakeCommandLine(const std::filesystem::path& executable, const std::vector<std::string>& arguments) {
		std::wstring result = QuoteArgument(executable.wstring());
		for (const auto& argument : arguments) { result.push_back(L' '); result += QuoteArgument(std::filesystem::path(argument).wstring()); }
		return result;
	}

	bool RunProcess(
		const std::filesystem::path& executable,
		const std::vector<std::string>& arguments,
		const std::filesystem::path& workingDirectory,
		const std::filesystem::path& logPath,
		DWORD& exitCode,
		bool& timedOut) {
		timedOut = false;
		SECURITY_ATTRIBUTES security{};
		security.nLength = sizeof(security);
		security.bInheritHandle = TRUE;
		HANDLE logHandle = CreateFileW(logPath.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (logHandle == INVALID_HANDLE_VALUE) return false;
		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		startup.dwFlags = STARTF_USESTDHANDLES;
		startup.hStdOutput = logHandle;
		startup.hStdError = logHandle;
		startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		PROCESS_INFORMATION process{};
		auto commandLine = MakeCommandLine(executable, arguments);
		const BOOL created = CreateProcessW(executable.wstring().c_str(), commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, workingDirectory.wstring().c_str(), &startup, &process);
		CloseHandle(logHandle);
		if (!created) return false;
		CloseHandle(process.hThread);
		const DWORD waitResult = WaitForSingleObject(process.hProcess, CompileTimeoutMilliseconds);
		if (waitResult == WAIT_TIMEOUT) { timedOut = true; TerminateProcess(process.hProcess, ERROR_TIMEOUT); WaitForSingleObject(process.hProcess, 1000); }
		const bool completed = GetExitCodeProcess(process.hProcess, &exitCode) != 0;
		CloseHandle(process.hProcess);
		return completed;
	}
#endif

	std::string ReadText(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::in | std::ios::binary);
		return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
	}

	HE::ResultEnvelope Failure(const std::filesystem::path& target, std::string message) {
		auto result = HE::ResultEnvelope::Failure("shader.dxc.compile", target.generic_string(), message);
		result.AddDetail({ HE::DiagnosticSeverity::Error, "shader.dxc.failed", std::move(message), target.generic_string() });
		return result;
	}
}

namespace HE::Rendering {
	std::filesystem::path DxcShaderCompiler::ResolveToolDirectory() const {
		return ResourcePaths::GetExecutableDirectory() / "ShaderTools" / "DXC";
	}

	ResultEnvelope DxcShaderCompiler::QueryCompilerIdentity(std::string& output) const {
		output.clear();
#ifdef HE_PLATFORM_WINDOWS
		const auto directory = ResolveToolDirectory();
		const auto executable = directory / "dxc.exe";
		const auto versionFile = directory / "VERSION";
		if (!std::filesystem::is_regular_file(executable) || !std::filesystem::is_regular_file(directory / "dxcompiler.dll") || !std::filesystem::is_regular_file(directory / "dxil.dll") || !std::filesystem::is_regular_file(versionFile)) return Failure(executable, "Fixed DXC distribution is incomplete");
		const auto jobRoot = std::filesystem::temp_directory_path() / "HuaEngine" / "ShaderCompile" / std::to_string(GetCurrentProcessId()) / std::to_string(NextJobId.fetch_add(1));
		std::error_code errorCode;
		std::filesystem::create_directories(jobRoot, errorCode);
		if (errorCode) return Failure(jobRoot, "Failed to create shader compiler job directory");
		ScopedJobDirectory cleanup(jobRoot);
		DWORD exitCode = 0;
		bool timedOut = false;
		const auto logPath = jobRoot / "version.log";
		if (!RunProcess(executable, { "--version" }, directory, logPath, exitCode, timedOut) || timedOut || exitCode != 0) return Failure(executable, timedOut ? "DXC version query timed out" : "DXC version query failed");
		std::ifstream versionStream(versionFile);
		std::string expectedVersion;
		std::getline(versionStream, expectedVersion);
		const auto actualVersion = ReadText(logPath);
		if (expectedVersion.empty() || actualVersion.find(expectedVersion) == std::string::npos) return Failure(executable, "DXC VERSION does not match the compiler binary");
		output = expectedVersion + "|" + actualVersion;
		return ResultEnvelope::Success("shader.dxc.identity", executable.generic_string(), "DXC identity verified");
#else
		return Failure({}, "DXC subprocess compilation is only supported on Windows");
#endif
	}

	ResultEnvelope DxcShaderCompiler::Compile(const DxcCompileRequest& request, DxcCompileOutput& output) const {
		output = {};
		const std::string stageName = request.Stage == ShaderStage::Vertex ? "vertex" : "fragment";
		if (!std::filesystem::is_regular_file(request.SourcePath) || request.EntryPoint.empty() || request.Profile.empty()) return Failure(request.SourcePath, stageName + " stage compile request is invalid");
		auto identityResult = QueryCompilerIdentity(output.CompilerIdentity);
		if (!identityResult.Succeeded()) return identityResult;
#ifdef HE_PLATFORM_WINDOWS
		const auto directory = ResolveToolDirectory();
		const auto executable = directory / "dxc.exe";
		const auto jobRoot = std::filesystem::temp_directory_path() / "HuaEngine" / "ShaderCompile" / std::to_string(GetCurrentProcessId()) / std::to_string(NextJobId.fetch_add(1));
		std::error_code errorCode;
		std::filesystem::create_directories(jobRoot, errorCode);
		if (errorCode) return Failure(jobRoot, "Failed to create shader compiler job directory");
		ScopedJobDirectory cleanup(jobRoot);
		const auto binaryPath = jobRoot / "stage.spv";
		const auto assemblyPath = jobRoot / "stage.spvasm";
		const auto logPath = jobRoot / "diagnostics.log";
		output.CompileOptions = { "-spirv", "-fspv-target-env=vulkan1.2", "-fspv-reflect", "-fvk-use-gl-layout", "-Zpc", "-WX", "-Ges", "-T", request.Profile, "-E", request.EntryPoint };
#ifdef _DEBUG
		output.CompileOptions.insert(output.CompileOptions.end(), { "-Zi", "-Od", "-fspv-debug=vulkan-with-source" });
#else
		output.CompileOptions.insert(output.CompileOptions.end(), { "-O3", "-Qstrip_debug" });
#endif
		std::vector<std::string> arguments = output.CompileOptions;
		arguments.insert(arguments.end(), { "-Fo", binaryPath.string(), "-Fc", assemblyPath.string() });
		for (const auto& includeRoot : request.IncludeRoots) { arguments.emplace_back("-I"); arguments.emplace_back(includeRoot.string()); }
		arguments.emplace_back(request.SourcePath.string());
		DWORD exitCode = 0;
		bool timedOut = false;
		if (!RunProcess(executable, arguments, request.SourcePath.parent_path(), logPath, exitCode, timedOut)) return Failure(request.SourcePath, "Failed to start DXC");
		if (std::filesystem::file_size(logPath, errorCode) > 1024 * 1024) return Failure(request.SourcePath, stageName + " stage diagnostic output exceeded 1 MiB");
		const auto diagnostics = ReadText(logPath);
		if (timedOut || exitCode != 0) return Failure(request.SourcePath, timedOut ? stageName + " stage compilation timed out" : stageName + " stage: " + diagnostics);
		std::ifstream stream(binaryPath, std::ios::in | std::ios::binary | std::ios::ate);
		const auto byteSize = stream.tellg();
		if (byteSize < 20 || byteSize % 4 != 0 || byteSize > 64 * 1024 * 1024) return Failure(request.SourcePath, "DXC produced an invalid SPIR-V module size");
		output.Spirv.resize(static_cast<size_t>(byteSize) / sizeof(uint32_t));
		stream.seekg(0);
		stream.read(reinterpret_cast<char*>(output.Spirv.data()), byteSize);
		if (!stream.good() || output.Spirv.front() != 0x07230203u) return Failure(request.SourcePath, "DXC output does not contain SPIR-V magic");
		output.Disassembly = ReadText(assemblyPath);
		return ResultEnvelope::Success("shader.dxc.compile", request.SourcePath.generic_string(), "HLSL stage compiled to SPIR-V");
#else
		return Failure(request.SourcePath, "DXC subprocess compilation is only supported on Windows");
#endif
	}
}
