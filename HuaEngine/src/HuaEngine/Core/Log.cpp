#include "enginepch.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace HE {
	Ref<spdlog::logger> Log::ms_CoreLogger;
	Ref<spdlog::logger> Log::ms_ClientLogger;
	Ref<LogSink> Log::ms_LogSink;

	void Log::Init() {
		ms_LogSink = std::make_shared<LogSink>();
		auto coreConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		coreConsoleSink->set_pattern("%^[%T] %n: %v%$");

		std::vector<spdlog::sink_ptr> coreSinks = { ms_LogSink, coreConsoleSink };
		ms_CoreLogger = std::make_shared<spdlog::logger>("HUA_ENGINE", coreSinks.begin(), coreSinks.end());
		ms_CoreLogger->set_level(spdlog::level::trace);

		auto clientConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		clientConsoleSink->set_pattern("%^[%T] %n: %v%$");
		std::vector<spdlog::sink_ptr> clientSinks = { ms_LogSink, clientConsoleSink };
		ms_ClientLogger = std::make_shared<spdlog::logger>("APP", clientSinks.begin(), clientSinks.end());
		ms_ClientLogger->set_level(spdlog::level::trace);
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