#include "enginepch.h"
#include "ResourcePaths.h"

#include <system_error>

#ifdef HE_PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace {
	std::filesystem::path NormalizePath(const std::filesystem::path& path) {
		if (path.empty()) {
			return {};
		}

		std::error_code errorCode;
		auto absolutePath = std::filesystem::absolute(path, errorCode);
		if (errorCode) {
			return path.lexically_normal();
		}

		if (std::filesystem::exists(absolutePath, errorCode)) {
			auto canonicalPath = std::filesystem::weakly_canonical(absolutePath, errorCode);
			if (!errorCode) {
				return canonicalPath;
			}
		}

		return absolutePath.lexically_normal();
	}
}

namespace HE {
	std::filesystem::path ResourcePaths::GetExecutablePath() {
#ifdef HE_PLATFORM_WINDOWS
		std::wstring buffer(MAX_PATH, L'\0');
		auto size = static_cast<DWORD>(buffer.size());
		auto copied = GetModuleFileNameW(nullptr, buffer.data(), size);
		while (copied >= size) {
			buffer.resize(buffer.size() * 2);
			size = static_cast<DWORD>(buffer.size());
			copied = GetModuleFileNameW(nullptr, buffer.data(), size);
		}

		if (copied == 0) {
			return {};
		}

		buffer.resize(copied);
		return NormalizePath(std::filesystem::path(buffer));
#else
		return {};
#endif
	}

	std::filesystem::path ResourcePaths::GetExecutableDirectory() {
		return GetExecutablePath().parent_path();
	}

	std::filesystem::path ResourcePaths::GetEngineResourceRoot() {
		const auto executableRoot = GetExecutableDirectory() / "Resources";
		std::error_code errorCode;
		if (std::filesystem::exists(executableRoot, errorCode)) {
			return NormalizePath(executableRoot);
		}

		const auto currentRoot = std::filesystem::current_path(errorCode) / "Resources";
		if (!errorCode && std::filesystem::exists(currentRoot, errorCode)) {
			return NormalizePath(currentRoot);
		}

		return NormalizePath(executableRoot);
	}

	std::filesystem::path ResourcePaths::ResolveRuntimePath(const std::filesystem::path& path) {
		if (path.empty()) {
			return {};
		}

		if (path.is_absolute()) {
			return NormalizePath(path);
		}

		std::error_code errorCode;
		const auto currentCandidate = std::filesystem::current_path(errorCode) / path;
		if (!errorCode && std::filesystem::exists(currentCandidate, errorCode)) {
			return NormalizePath(currentCandidate);
		}

		const auto executableCandidate = GetExecutableDirectory() / path;
		if (std::filesystem::exists(executableCandidate, errorCode)) {
			return NormalizePath(executableCandidate);
		}

		return path.lexically_normal();
	}

	std::filesystem::path ResourcePaths::ResolveEngineResourcePath(const std::filesystem::path& relativePath) {
		return NormalizePath(GetEngineResourceRoot() / relativePath);
	}
}
