#include "enginepch.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace HE {
	std::shared_ptr<spdlog::logger> Log::ms_CoreLogger;
	std::shared_ptr<spdlog::logger> Log::ms_ClientLogger;

	void Log::Init() {
		spdlog::set_pattern("%^[%T] %n: %v%$");

		ms_CoreLogger = spdlog::stdout_color_mt("HUA_ENGINE");
		ms_CoreLogger->set_level(spdlog::level::trace);

		ms_ClientLogger = spdlog::stdout_color_mt("APP");
		ms_ClientLogger->set_level(spdlog::level::trace);
	}
}