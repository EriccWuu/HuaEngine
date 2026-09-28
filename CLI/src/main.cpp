#include "enginepch.h"
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "CLIApplication.h"
#include "CLICommandRunner.h"
#include "CLIJsonWriter.h"
#include "HuaEngine/Core/Log.h"

#ifdef _WIN32
namespace {
	std::string ArgumentToUtf8(const wchar_t* argument) {
		const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argument, -1, nullptr, 0, nullptr, nullptr);
		if (required <= 0) {
			throw std::invalid_argument("Command-line argument contains invalid UTF-16");
		}
		std::string result(static_cast<size_t>(required), '\0');
		if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argument, -1, result.data(), required, nullptr, nullptr) != required) {
			throw std::runtime_error("Could not encode command-line argument as UTF-8");
		}
		result.pop_back();
		return result;
	}
}

int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
	try {
		HE::Log::Init({ .EnableConsoleOutput = false });

		HE::CLI::CLIApplication application;
		application.Start();

		std::vector<std::string> arguments;
		arguments.reserve(static_cast<size_t>(argc > 0 ? argc - 1 : 0));
		for (int index = 1; index < argc; ++index) {
#ifdef _WIN32
			arguments.emplace_back(ArgumentToUtf8(argv[index]));
#else
			arguments.emplace_back(argv[index]);
#endif
		}

		HE::CLI::CommandRunner runner(application.GetOperations());
		auto response = runner.Run(arguments, std::filesystem::current_path());
		std::cout << HE::CLI::RenderJson(response) << std::endl;
		return HE::CLI::ExitCodeFor(response.Result);
	}
	catch (const std::exception& exception) {
		HE::ResultEnvelope result = HE::ResultEnvelope::Failure("cli.exception", "command_line", "Unhandled exception during cli execution");
		result.AddDetail({ HE::DiagnosticSeverity::Error, "cli.exception.std", exception.what(), {} });
		std::cout << HE::CLI::RenderJson({ std::move(result) }) << std::endl;
		return 70;
	}
	catch (...) {
		HE::ResultEnvelope result = HE::ResultEnvelope::Failure("cli.exception", "command_line", "Unhandled non-standard exception during cli execution");
		result.AddDetail({ HE::DiagnosticSeverity::Error, "cli.exception.unknown", "Unknown exception type", {} });
		std::cout << HE::CLI::RenderJson({ std::move(result) }) << std::endl;
		return 70;
	}
}
