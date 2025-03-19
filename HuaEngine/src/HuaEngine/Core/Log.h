#pragma once
#include "HuaEngine/Core/Core.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace HE {
	class ENGINE_API Log {
	public:
		static void Init();
		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return ms_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return ms_ClientLogger; }
	
	private:
		static std::shared_ptr<spdlog::logger> ms_CoreLogger;
		static std::shared_ptr<spdlog::logger> ms_ClientLogger;
	};
}

// Log macros
#define HE_CORE_TRACE(...)       ::HE::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define HE_CORE_INFO(...)        ::HE::Log::GetCoreLogger()->info(__VA_ARGS__)
#define HE_CORE_WARN(...)        ::HE::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define HE_CORE_ERROR(...)       ::HE::Log::GetCoreLogger()->error(__VA_ARGS__)
#define HE_CORE_CRITICAL(...)    ::HE::Log::GetCoreLogger()->critical(__VA_ARGS__)

#define HE_TRACE(...)            ::HE::Log::GetClientLogger()->trace(__VA_ARGS__)
#define HE_INFO(...)             ::HE::Log::GetClientLogger()->info(__VA_ARGS__)
#define HE_WARN(...)             ::HE::Log::GetClientLogger()->warn(__VA_ARGS__)
#define HE_ERROR(...)            ::HE::Log::GetClientLogger()->error(__VA_ARGS__)
#define HE_CRITICAL(...)         ::HE::Log::GetClientLogger()->critical(__VA_ARGS__)

