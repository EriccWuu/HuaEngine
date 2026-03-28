#include "enginepch.h"
#include "HuaEngine/Core/ResourcePaths.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace {
	std::filesystem::path ResolveDefaultLogDirectory() {
		const char* localAppData = std::getenv("LOCALAPPDATA");
		if (localAppData && *localAppData) {
			return std::filesystem::path(localAppData) / "HuaEngine" / "Logs";
		}

		std::error_code errorCode;
		const auto tempRoot = std::filesystem::temp_directory_path(errorCode);
		if (!errorCode) {
			return tempRoot / "HuaEngine" / "Logs";
		}

		return std::filesystem::current_path(errorCode) / "Logs";
	}

	std::filesystem::path ResolveLogDirectory(const HE::LogSpecification& specification) {
		if (!specification.LogDirectory.empty()) {
			return specification.LogDirectory;
		}

		return ResolveDefaultLogDirectory();
	}

	std::filesystem::path ResolveLogFilePath(const std::filesystem::path& logDirectory) {
		auto executablePath = HE::ResourcePaths::GetExecutablePath();
		auto executableName = executablePath.stem();
		if (executableName.empty()) {
			executableName = "HuaEngine";
		}

		return logDirectory / (executableName.string() + ".log");
	}
}

namespace HE {
	Ref<spdlog::logger> Log::ms_CoreLogger;
	Ref<spdlog::logger> Log::ms_ClientLogger;
	Ref<LogSink> Log::ms_LogSink;
	std::filesystem::path Log::ms_LogFilePath;

	void Log::Init(const LogSpecification& specification) {
		ms_LogSink = std::make_shared<LogSink>();
		std::vector<spdlog::sink_ptr> coreSinks = { ms_LogSink };
		ms_LogFilePath.clear();

		if (specification.EnableFileOutput) {
			std::error_code errorCode;
			const auto logDirectory = ResolveLogDirectory(specification);
			std::filesystem::create_directories(logDirectory, errorCode);
			if (!errorCode) {
				ms_LogFilePath = ResolveLogFilePath(logDirectory);
				auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(ms_LogFilePath.string(), true);
				fileSink->set_pattern("[%Y-%m-%d %T.%e] [%n] [%l] %v");
				coreSinks.emplace_back(fileSink);
			}
		}

		if (specification.EnableConsoleOutput) {
			auto coreConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			coreConsoleSink->set_pattern("%^[%T] %n: %v%$");
			coreSinks.emplace_back(coreConsoleSink);
		}
		ms_CoreLogger = std::make_shared<spdlog::logger>("HUA_ENGINE", coreSinks.begin(), coreSinks.end());
		ms_CoreLogger->set_level(spdlog::level::trace);
		ms_CoreLogger->flush_on(spdlog::level::trace);

		std::vector<spdlog::sink_ptr> clientSinks = { ms_LogSink };
		if (specification.EnableFileOutput && coreSinks.size() > 1) {
			for (const auto& sink : coreSinks) {
				if (sink != ms_LogSink) {
					clientSinks.emplace_back(sink);
				}
			}
		}
		if (specification.EnableConsoleOutput) {
			bool hasConsoleSink = false;
			for (const auto& sink : clientSinks) {
				if (std::dynamic_pointer_cast<spdlog::sinks::stdout_color_sink_mt>(sink)) {
					hasConsoleSink = true;
					break;
				}
			}
			if (!hasConsoleSink) {
				auto clientConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
				clientConsoleSink->set_pattern("%^[%T] %n: %v%$");
				clientSinks.emplace_back(clientConsoleSink);
			}
		}
		ms_ClientLogger = std::make_shared<spdlog::logger>("APP", clientSinks.begin(), clientSinks.end());
		ms_ClientLogger->set_level(spdlog::level::trace);
		ms_ClientLogger->flush_on(spdlog::level::trace);

		if (!ms_LogFilePath.empty()) {
			ms_CoreLogger->info("File logging initialized at {}", ms_LogFilePath.string());
		}
	}

	void LogSink::sink_it_(const spdlog::details::log_msg& msg) {
		spdlog::memory_buf_t formatted;
		formatter_->format(msg, formatted);

		buffer.push_back({
			msg.level,
			fmt::to_string(formatted)
		});
	}
}
