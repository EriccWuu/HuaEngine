#pragma once
#include "HuaEngine/Core/Core.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace HE {
	class LogSink : public spdlog::sinks::base_sink<std::mutex> {
	public:
		struct LogLine {
			spdlog::level::level_enum level;
			std::string message;
		};

		const std::vector<LogLine>& GetBuffer() const { return buffer; }
		void Clear() { buffer.clear(); }

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override;

		void flush_() override {}

	private:
		std::vector<LogLine> buffer;
	};

	class ENGINE_API Log {
	public:
		static void Init();
		inline static Ref<spdlog::logger>& GetCoreLogger() { return ms_CoreLogger; }
		inline static Ref<spdlog::logger>& GetClientLogger() { return ms_ClientLogger; }
		inline static Ref<LogSink>& GetLogSink() { return ms_LogSink; }
	
	private:
		static Ref<spdlog::logger> ms_CoreLogger;
		static Ref<spdlog::logger> ms_ClientLogger;
		static Ref<LogSink> ms_LogSink;
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

