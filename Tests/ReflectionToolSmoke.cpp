#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <process.h>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {
    std::wstring Wide(const std::string& text) {
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0) throw std::runtime_error("Invalid UTF-8 argument");
        std::wstring result(length, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), length);
        return result;
    }
}

int main() {
    try {
        const char* python = std::getenv("HUAENGINE_PYTHON_EXECUTABLE");
        if (!python || !std::filesystem::is_regular_file(python)) throw std::runtime_error("CTest must provide the locked Python path");
        std::vector<std::wstring> arguments{
            Wide(python), Wide(std::string(HUAENGINE_TEST_SOURCE_ROOT) + "/Tests/Meta/ReflectionToolSmoke.py"),
            L"--core", Wide(HUAENGINE_TEST_META_CORE_CONFIG),
            L"--rendering", Wide(HUAENGINE_TEST_META_RENDERING_CONFIG)
        };
        std::vector<const wchar_t*> pointers;
        for (const auto& argument : arguments) pointers.push_back(argument.c_str());
        pointers.push_back(nullptr);
        const auto status = _wspawnv(_P_WAIT, arguments.front().c_str(), pointers.data());
        if (status != 0) throw std::runtime_error("Real reflection tool validation failed");
        std::cout << "ReflectionToolSmoke passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "[ReflectionToolSmoke] " << exception.what() << '\n';
        return 1;
    }
}
