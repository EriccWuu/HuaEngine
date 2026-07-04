#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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
		std::string Command;
		std::string Output;
	};

	void Expect(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ReflectionToolSmoke] " << message << std::endl;
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

	std::string QuoteForDisplay(std::string_view argument) {
		if (!argument.empty() && argument.find_first_of(" \t\n\v\"") == std::string_view::npos) {
			return std::string(argument);
		}

		std::string quoted = "\"";
		for (const char character : argument) {
			if (character == '"' || character == '\\') {
				quoted += '\\';
			}
			quoted += character;
		}

		quoted += "\"";
		return quoted;
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

	std::wstring BuildCommandLine(const std::vector<std::string>& arguments) {
		std::wstring commandLine;
		for (const auto& argument : arguments) {
			if (!commandLine.empty()) {
				commandLine += L" ";
			}
			commandLine += QuoteForCommandLine(Utf8ToWide(argument));
		}

		return commandLine;
	}

	std::string BuildDisplayCommand(const std::vector<std::string>& arguments) {
		std::string command;
		for (const auto& argument : arguments) {
			if (!command.empty()) {
				command += " ";
			}
			command += QuoteForDisplay(argument);
		}

		return command;
	}

	ProcessResult RunCommand(
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
		std::wstring commandLine = BuildCommandLine(arguments);
		std::wstring workingDirectoryWide = workingDirectory.wstring();

		const BOOL created = CreateProcessW(
			nullptr,
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
		Expect(created == TRUE, "Failed to launch command: " + BuildDisplayCommand(arguments));

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

		return { exitCode, BuildDisplayCommand(arguments), std::move(output) };
	}

	void ExpectCommandSucceeded(const ProcessResult& result, const std::string& label) {
		Expect(
			result.ExitCode == 0,
			label + " should exit with code 0\nCommand: " + result.Command + "\nExit code: " +
				std::to_string(result.ExitCode) + "\nOutput:\n" + result.Output);
	}

	void ExpectCommandFailedWith(
		const ProcessResult& result,
		const std::string& label,
		std::string_view expectedDiagnostic) {
		Expect(
			result.ExitCode != 0,
			label + " should exit with a non-zero code\nCommand: " + result.Command + "\nOutput:\n" + result.Output);
		Expect(
			result.Output.find(expectedDiagnostic) != std::string::npos,
			label + " should report diagnostic " + std::string(expectedDiagnostic) + "\nOutput:\n" + result.Output);
	}

	void WriteTextFile(const std::filesystem::path& path, std::string_view content) {
		std::error_code errorCode;
		std::filesystem::create_directories(path.parent_path(), errorCode);
		Expect(!errorCode, "Failed to create fixture directory: " + path.parent_path().string());

		std::ofstream stream(path, std::ios::binary);
		Expect(stream.is_open(), "Failed to open fixture file: " + path.string());
		stream << content;
		stream.close();
		Expect(stream.good(), "Failed to write fixture file: " + path.string());
	}

	std::string ReadTextFile(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::binary);
		Expect(stream.is_open(), "Failed to open text file: " + path.string());
		return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
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

	std::filesystem::path FindRepositoryRoot(std::filesystem::path start) {
		std::error_code errorCode;
		start = std::filesystem::absolute(std::move(start), errorCode);
		Expect(!errorCode, "Failed to resolve current directory");

		for (std::filesystem::path candidate = start; !candidate.empty(); candidate = candidate.parent_path()) {
			if (std::filesystem::exists(candidate / "Tools" / "Reflection" / "reflection_tool.py")) {
				return candidate;
			}

			if (candidate == candidate.parent_path()) {
				break;
			}
		}

		Expect(false, "Failed to locate repository root");
		return {};
	}

	void ValidateNegativeFixture(
		const std::filesystem::path& reflectionToolPath,
		const std::filesystem::path& workspaceRoot,
		const std::string& fixtureName,
		std::string_view source,
		std::string_view expectedDiagnostic) {
		const auto fixtureRoot = workspaceRoot / fixtureName;
		std::error_code errorCode;
		std::filesystem::remove_all(fixtureRoot, errorCode);
		Expect(!errorCode, "Failed to clean negative fixture: " + fixtureRoot.string());
		WriteTextFile(fixtureRoot / "HuaEngine" / "src" / "Fixture" / "FixtureComponent.h", source);

		const auto result = RunCommand(
			{ "python", reflectionToolPath.string(), "validate", "--root", fixtureRoot.string() },
			fixtureRoot);
		ExpectCommandFailedWith(result, fixtureName, expectedDiagnostic);
	}

	void ValidateGeneratedDriftFixture(
		const std::filesystem::path& reflectionToolPath,
		const std::filesystem::path& workspaceRoot) {
		const auto fixtureRoot = workspaceRoot / "generated_drift";
		std::error_code errorCode;
		std::filesystem::remove_all(fixtureRoot, errorCode);
		Expect(!errorCode, "Failed to clean generated drift fixture");
		WriteTextFile(
			fixtureRoot / "HuaEngine" / "src" / "Fixture" / "DriftComponent.h",
			"namespace HE {\n"
			"HE_REFLECT_" "COMPONENT(DisplayName=\"Drift\", Category=\"Fixture\")\n"
			"struct DriftComponent {\n"
			"    HE_REFLECT_" "FIELD()\n"
			"    int Value = 1;\n"
			"};\n"
			"}\n");
		WriteTextFile(
			fixtureRoot / "HuaEngine" / "src" / "HuaEngine" / "Generated" / "GeneratedReflection.h",
			"// stale generated header\n");
		WriteTextFile(
			fixtureRoot / "HuaEngine" / "src" / "HuaEngine" / "Generated" / "GeneratedReflection.cpp",
			"// stale generated source\n");

		const auto result = RunCommand(
			{ "python", reflectionToolPath.string(), "validate", "--root", fixtureRoot.string() },
			fixtureRoot);
		ExpectCommandFailedWith(result, "generated drift fixture", "generated.drift");
	}

	void RunNegativeValidationSmoke(
		const std::filesystem::path& repositoryRoot,
		const std::filesystem::path& reflectionToolPath) {
		const auto workspaceRoot = repositoryRoot / ".workspace" / "reflection_negative_smoke";
		RegisterCleanupPath(workspaceRoot);
		std::error_code errorCode;
		std::filesystem::create_directories(workspaceRoot, errorCode);
		Expect(!errorCode, "Failed to prepare reflection negative smoke workspace");

		ValidateNegativeFixture(
			reflectionToolPath,
			workspaceRoot,
			"missing_display_name",
			"namespace HE {\n"
			"HE_REFLECT_" "COMPONENT(Category=\"Fixture\")\n"
			"struct MissingDisplayNameComponent {\n"
			"    HE_REFLECT_" "FIELD()\n"
			"    int Value = 1;\n"
			"};\n"
			"}\n",
			"component.missing_display_name");

		ValidateNegativeFixture(
			reflectionToolPath,
			workspaceRoot,
			"missing_category",
			"namespace HE {\n"
			"HE_REFLECT_" "COMPONENT(DisplayName=\"Missing Category\")\n"
			"struct MissingCategoryComponent {\n"
			"    HE_REFLECT_" "FIELD()\n"
			"    int Value = 1;\n"
			"};\n"
			"}\n",
			"component.missing_category");

		ValidateNegativeFixture(
			reflectionToolPath,
			workspaceRoot,
			"empty_fields",
			"namespace HE {\n"
			"HE_REFLECT_" "COMPONENT(DisplayName=\"Empty\", Category=\"Fixture\")\n"
			"struct EmptyFieldsComponent {\n"
			"};\n"
			"}\n",
			"component.no_reflected_fields");

		ValidateNegativeFixture(
			reflectionToolPath,
			workspaceRoot,
			"invalid_field",
			"namespace HE {\n"
			"HE_REFLECT_" "COMPONENT(DisplayName=\"Invalid\", Category=\"Fixture\")\n"
			"struct InvalidFieldComponent {\n"
			"    HE_REFLECT_" "FIELD()\n"
			"    void NotAField();\n"
			"};\n"
			"}\n",
			"field.unparsed_declaration");

		const auto duplicateRoot = workspaceRoot / "duplicate_type";
		std::filesystem::remove_all(duplicateRoot, errorCode);
		Expect(!errorCode, "Failed to clean duplicate type fixture");
		WriteTextFile(
			duplicateRoot / "HuaEngine" / "src" / "A" / "DuplicateComponent.h",
			"namespace HE {\n"
			"HE_REFLECT_" "COMPONENT(DisplayName=\"Duplicate A\", Category=\"Fixture\")\n"
			"struct DuplicateComponent {\n"
			"    HE_REFLECT_" "FIELD()\n"
			"    int A = 1;\n"
			"};\n"
			"}\n");
		WriteTextFile(
			duplicateRoot / "HuaEngine" / "src" / "B" / "DuplicateComponent.h",
			"namespace HE {\n"
			"HE_REFLECT_" "COMPONENT(DisplayName=\"Duplicate B\", Category=\"Fixture\")\n"
			"struct DuplicateComponent {\n"
			"    HE_REFLECT_" "FIELD()\n"
			"    int B = 2;\n"
			"};\n"
			"}\n");
		const auto duplicateResult = RunCommand(
			{ "python", reflectionToolPath.string(), "validate", "--root", duplicateRoot.string() },
			duplicateRoot);
		ExpectCommandFailedWith(duplicateResult, "duplicate type fixture", "component.duplicate_qualified_name");

		const auto enumRoot = workspaceRoot / "enum_positive";
		std::filesystem::remove_all(enumRoot, errorCode);
		Expect(!errorCode, "Failed to clean enum positive fixture");
		WriteTextFile(
			enumRoot / "HuaEngine" / "src" / "Fixture" / "EnumComponent.h",
			"namespace HE {\n"
			"HE_REFLECT_" "ENUM()\n"
			"enum class FixtureMode {\n"
			"    A,\n"
			"    B = 4,\n"
			"    C,\n"
			"    D = -1,\n"
			"    E = 0x10\n"
			"};\n"
			"HE_REFLECT_" "COMPONENT(DisplayName=\"Enum Component\", Category=\"Fixture\")\n"
			"struct EnumComponent {\n"
			"    HE_REFLECT_" "FIELD()\n"
			"    FixtureMode Mode = FixtureMode::A;\n"
			"};\n"
			"}\n");
		const auto enumResult = RunCommand(
			{ "python", reflectionToolPath.string(), "validate", "--root", enumRoot.string() },
			enumRoot);
		ExpectCommandSucceeded(enumResult, "enum positive fixture");
		Expect(enumResult.Output.find("\"enums\"") != std::string::npos, "enum positive fixture should include enums in manifest");
		Expect(enumResult.Output.find("\"FixtureMode\"") != std::string::npos, "enum positive fixture should include FixtureMode metadata");

		ValidateNegativeFixture(
			reflectionToolPath,
			workspaceRoot,
			"enum_complex_expression",
			"namespace HE {\n"
			"HE_REFLECT_" "ENUM()\n"
			"enum class BadFlags {\n"
			"    A = 1 << 0\n"
			"};\n"
			"}\n",
			"enum.value_unsupported_expression");

		ValidateGeneratedDriftFixture(reflectionToolPath, workspaceRoot);
	}

	void ExpectNoLegacyGeneratedIncludeMarkers(const std::filesystem::path& path, const std::string& label) {
		const std::string content = ReadTextFile(path);
		Expect(
			content.find("Generated/Reflection") == std::string::npos,
			label + " should not include legacy per-source generated reflection headers");
		Expect(
			content.find("HE_GENERATED_REFLECTION_SOURCE_") == std::string::npos,
			label + " should not contain legacy generated reflection source macro wrappers");
		Expect(
			content.find(".generated.h") == std::string::npos,
			label + " should not include generated reflection header fragments");
	}
}

int main() {
	(void)CleanupPaths();
	std::atexit(CleanupRegisteredPaths);

	const auto binaryDirectory = GetCurrentExecutablePath().parent_path();
	const auto repositoryRoot = FindRepositoryRoot(binaryDirectory);
	const auto reflectionToolPath = repositoryRoot / "Tools" / "Reflection" / "reflection_tool.py";
	RegisterCleanupPath(reflectionToolPath.parent_path() / "__pycache__");

	std::error_code errorCode;
	const auto workspaceReflectionDirectory = repositoryRoot / ".workspace" / "reflection";
	std::filesystem::create_directories(workspaceReflectionDirectory, errorCode);
	Expect(!errorCode, "Failed to prepare .workspace/reflection directory");

	const auto manifestPath = workspaceReflectionDirectory / "reflection_tool_smoke_manifest.json";
	const auto validatePath = workspaceReflectionDirectory / "reflection_tool_validate.json";
	RegisterCleanupPath(manifestPath);
	RegisterCleanupPath(validatePath);
	std::filesystem::remove(manifestPath, errorCode);
	std::filesystem::remove(validatePath, errorCode);

	const auto scanResult = RunCommand(
		{ "python", reflectionToolPath.string(), "scan", "--root", repositoryRoot.string(), "--out", manifestPath.string() },
		repositoryRoot);
	ExpectCommandSucceeded(scanResult, "reflection tool scan");
	Expect(std::filesystem::exists(manifestPath), "reflection tool scan should write the smoke manifest");

	const auto validateResult = RunCommand(
		{ "python", reflectionToolPath.string(), "validate", "--root", repositoryRoot.string() },
		repositoryRoot);
	ExpectCommandSucceeded(validateResult, "reflection tool validate");

	std::ofstream validateOutput(validatePath, std::ios::binary);
	Expect(validateOutput.is_open(), "Failed to open reflection tool validate output file");
	validateOutput << validateResult.Output;
	validateOutput.close();
	Expect(validateOutput.good(), "Failed to write reflection tool validate output file");

	RunNegativeValidationSmoke(repositoryRoot, reflectionToolPath);

	const auto generatedReflectionDirectory = repositoryRoot / "HuaEngine" / "src" / "HuaEngine" / "Generated";
	ExpectNoLegacyGeneratedIncludeMarkers(
		repositoryRoot / "HuaEngine" / "src" / "HuaEngine" / "ECS" / "Components.h",
		"Components.h");
	ExpectNoLegacyGeneratedIncludeMarkers(
		repositoryRoot / "HuaEngine" / "src" / "Module" / "Rendering" / "RenderingComponent.h",
		"RenderingComponent.h");
	Expect(
		ReadTextFile(generatedReflectionDirectory / "GeneratedReflection.h").find("srefl_class(") == std::string::npos,
		"GeneratedReflection.h should contain runtime descriptor declarations only; generated component implementations belong in GeneratedReflection.cpp");
	const std::string generatedSource = ReadTextFile(generatedReflectionDirectory / "GeneratedReflection.cpp");
	Expect(
		generatedSource.find("Serialize_HE__TransformComponent(") == std::string::npos,
		"GeneratedReflection.cpp should not emit ordinary component type-level serializers");
	Expect(
		generatedSource.find("Deserialize_HE__TransformComponent(") == std::string::npos,
		"GeneratedReflection.cpp should not emit ordinary component type-level deserializers");

	std::cout << "ReflectionToolSmoke passed" << std::endl;
	return 0;
}
