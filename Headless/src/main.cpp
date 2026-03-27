#include "enginepch.h"
#include <exception>
#include <filesystem>
#include <iostream>
#include <vector>

#include "HeadlessApplication.h"
#include "HeadlessCommandRunner.h"
#include "HeadlessJsonWriter.h"
#include "HuaEngine/Core/Log.h"

int main(int argc, char** argv) {
	try {
		HE::Log::Init({ .EnableConsoleOutput = false });

		HE::Headless::HeadlessApplication application;
		application.Start();

		std::vector<std::string> arguments;
		arguments.reserve(static_cast<size_t>(argc > 0 ? argc - 1 : 0));
		for (int index = 1; index < argc; ++index) {
			arguments.emplace_back(argv[index]);
		}

		HE::Headless::CommandRunner runner(application.GetOperations());
		auto response = runner.Run(arguments, std::filesystem::current_path());
		std::cout << HE::Headless::RenderJson(response) << std::endl;
		return HE::Headless::ExitCodeFor(response.Result);
	}
	catch (const std::exception& exception) {
		HE::ResultEnvelope result = HE::ResultEnvelope::Failure("cli.exception", "command_line", "Unhandled exception during headless execution");
		result.AddDetail({ HE::DiagnosticSeverity::Error, "cli.exception.std", exception.what(), {} });
		std::cout << HE::Headless::RenderJson({ std::move(result) }) << std::endl;
		return 70;
	}
	catch (...) {
		HE::ResultEnvelope result = HE::ResultEnvelope::Failure("cli.exception", "command_line", "Unhandled non-standard exception during headless execution");
		result.AddDetail({ HE::DiagnosticSeverity::Error, "cli.exception.unknown", "Unknown exception type", {} });
		std::cout << HE::Headless::RenderJson({ std::move(result) }) << std::endl;
		return 70;
	}
}
