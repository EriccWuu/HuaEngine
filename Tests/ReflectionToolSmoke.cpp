#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
	void Expect(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ReflectionToolSmoke] " << message << std::endl;
			std::exit(1);
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

	void RunCommand(const char* command, const std::string& label) {
		const int exitCode = std::system(command);
		Expect(exitCode == 0, label + " should exit with code 0, got " + std::to_string(exitCode));
	}
}

int main() {
	const auto repositoryRoot = FindRepositoryRoot(std::filesystem::current_path());
	std::error_code errorCode;
	std::filesystem::current_path(repositoryRoot, errorCode);
	Expect(!errorCode, "Failed to switch to repository root");

	const auto workspaceReflectionDirectory = repositoryRoot / ".workspace" / "reflection";
	std::filesystem::create_directories(workspaceReflectionDirectory, errorCode);
	Expect(!errorCode, "Failed to prepare .workspace/reflection directory");

	const auto manifestPath = workspaceReflectionDirectory / "reflection_tool_smoke_manifest.json";
	std::filesystem::remove(manifestPath, errorCode);

	RunCommand(
		"python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_tool_smoke_manifest.json",
		"reflection tool scan");
	Expect(std::filesystem::exists(manifestPath), "reflection tool scan should write the smoke manifest");

	RunCommand(
		"python Tools/Reflection/reflection_tool.py validate --root . > .workspace/reflection/reflection_tool_validate.json",
		"reflection tool validate");

	std::cout << "ReflectionToolSmoke passed" << std::endl;
	return 0;
}
