#pragma once

#include "HuaEngine/Application.h"
#include "HuaEngine/Core/Log.h"
#include "HuaEngine/Core/Sha256.h"
#include "HuaEngine/Serialization/JsonSerializationBackend.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <Windows.h>
#include <shellapi.h>
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Shell32.lib")

namespace HE::HostSmoke {
    inline void Require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }
    inline std::string HashFile(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        Require(static_cast<bool>(stream), "Cannot read evidence file: " + path.string());
        const std::vector<uint8_t> data{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        return Sha256ToHex(ComputeSha256(data));
    }
    inline std::wstring Quote(const std::wstring& value) {
        std::wstring result = L"\"";
        size_t slashes = 0;
        for (const wchar_t character : value) {
            if (character == L'\\') { ++slashes; continue; }
            result.append(character == L'"' ? slashes * 2 + 1 : slashes, L'\\');
            slashes = 0;
            result += character;
        }
        result.append(slashes * 2, L'\\');
        return result + L'"';
    }

    class ScopedEnvironment final {
    public:
        ScopedEnvironment() : m_OriginalDirectory(std::filesystem::current_path()) {}
        ~ScopedEnvironment() {
            std::error_code ignored;
            std::filesystem::current_path(m_OriginalDirectory, ignored);
            for (const auto& [name, value] : m_Previous) (void)_putenv_s(name.c_str(), value ? value->c_str() : "");
        }
        ScopedEnvironment(const ScopedEnvironment&) = delete;
        ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;
        void Isolate(const std::filesystem::path& workspace) {
            for (const auto* name : {"LOCALAPPDATA", "APPDATA", "TEMP", "TMP"}) {
                const char* previous = std::getenv(name);
                m_Previous.emplace_back(name, previous ? std::optional<std::string>(previous) : std::nullopt);
                const auto path = workspace / name;
                std::filesystem::create_directories(path);
                Require(_putenv_s(name, path.string().c_str()) == 0, "Cannot isolate a host environment variable");
            }
            // ImGui and other relative configuration files stay with this run.
            std::filesystem::current_path(workspace);
        }
    private:
        std::filesystem::path m_OriginalDirectory;
        std::vector<std::pair<std::string, std::optional<std::string>>> m_Previous;
    };

    class ChildProcess final {
    public:
        ~ChildProcess() {
            if (!m_Handle) return;
            if (WaitForSingleObject(m_Handle, 0) == WAIT_TIMEOUT) {
                TerminateProcess(m_Handle, 2);
                WaitForSingleObject(m_Handle, 3000);
            }
            CloseHandle(m_Handle);
        }
        ChildProcess() = default;
        ChildProcess(const ChildProcess&) = delete;
        ChildProcess& operator=(const ChildProcess&) = delete;
        void Start(const std::filesystem::path& executable, const std::vector<std::wstring>& arguments,
            const std::filesystem::path& workingDirectory) {
            Require(!m_Handle, "The smoke child was already started");
            std::wstring command = Quote(executable.wstring());
            for (const auto& argument : arguments) command += L' ' + Quote(argument);
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            const auto cwd = workingDirectory.wstring();
            Require(CreateProcessW(executable.wstring().c_str(), command.data(), nullptr, nullptr,
                FALSE, 0, nullptr, cwd.c_str(), &startup, &process) != FALSE, "Cannot launch the real Editor child process");
            m_Handle = process.hProcess;
            m_Id = process.dwProcessId;
            CloseHandle(process.hThread);
        }
        [[nodiscard]] bool Started() const noexcept { return m_Handle != nullptr; }
        [[nodiscard]] DWORD Id() const noexcept { return m_Id; }
        [[nodiscard]] bool Finished() const { return m_Handle && WaitForSingleObject(m_Handle, 0) == WAIT_OBJECT_0; }
        [[nodiscard]] DWORD ExitCode() const {
            DWORD result = STILL_ACTIVE;
            Require(m_Handle && GetExitCodeProcess(m_Handle, &result), "Cannot read the Editor child exit code");
            return result;
        }
    private:
        HANDLE m_Handle = nullptr;
        DWORD m_Id = 0;
    };

    struct Capture {
        std::filesystem::path Path;
        std::string Sha256;
        uint64_t DrawnPixels = 0;
        uint32_t Width = 0, Height = 0;
    };
    struct Session {
        std::string Host;
        std::filesystem::path Report;
        std::filesystem::path Workspace;
        std::filesystem::path SourceProject;
        uint32_t ProcessId = GetCurrentProcessId();
        uint32_t MainThreadId = GetCurrentThreadId();
        uint32_t GraphicsThreadId = 0;
        uint64_t Updates = 0, GuiFrames = 0, RenderFrames = 0, WindowEvents = 0, WorkerGraphicsCalls = 0;
        std::optional<std::chrono::steady_clock::time_point> PreviousUpdate;
        std::vector<double> FrameIntervalsMs;
        uint32_t ChildProcessId = 0;
        std::filesystem::path ChildExecutable, ChildReport;
        std::string ChildExecutableSha256;
        bool WorkflowComplete = false;
        std::string Error;
        std::vector<std::string> Events;
        std::vector<Capture> Captures;
        std::map<std::string, std::string> Snapshots;
        std::chrono::steady_clock::time_point Started = std::chrono::steady_clock::now();

        void PrepareWorkspace() {
            std::filesystem::create_directories(Report.parent_path());
            for (uint32_t attempt = 0; attempt != 10000; ++attempt) {
                auto candidate = Report.parent_path() / ("smoke-" + std::to_string(ProcessId) + "-" + std::to_string(attempt));
                if (std::filesystem::create_directory(candidate)) {
                    Workspace = std::move(candidate);
                    Snapshots["workspace_path"] = Workspace.generic_string();
                    return;
                }
            }
            throw std::runtime_error("Cannot allocate a fresh smoke workspace");
        }
        void CopyProject(const std::filesystem::path& source, const std::filesystem::path& destination) const {
            Require(std::filesystem::is_directory(source), "The smoke source project is not a directory");
            std::filesystem::create_directories(destination);
            for (auto entry = std::filesystem::recursive_directory_iterator(source);
                entry != std::filesystem::recursive_directory_iterator(); ++entry) {
                if (entry->is_directory() && std::filesystem::equivalent(entry->path(), Workspace)) {
                    entry.disable_recursion_pending();
                    continue;
                }
                const auto attributes = GetFileAttributesW(entry->path().wstring().c_str());
                Require(attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT),
                    "Smoke project copies do not follow symlinks or junctions");
                const auto target = destination / entry->path().lexically_relative(source);
                if (entry->is_directory()) std::filesystem::create_directories(target);
                else if (entry->is_regular_file()) std::filesystem::copy_file(entry->path(), target);
            }
        }

        void Event(std::string name) { Events.push_back(std::move(name)); }
        void Tick() {
            const auto now = std::chrono::steady_clock::now();
            if (PreviousUpdate) {
                FrameIntervalsMs.push_back(std::chrono::duration<double, std::milli>(now - *PreviousUpdate).count());
            }
            PreviousUpdate = now;
            ++Updates;
            Require(now - Started < std::chrono::seconds(110), "Host smoke exceeded its 110-second internal deadline");
        }
        void Graphics() {
            const auto thread = GetCurrentThreadId();
            GraphicsThreadId = thread;
            if (thread != MainThreadId) ++WorkerGraphicsCalls;
        }
        void CaptureWindow() {
            struct Search { DWORD Pid; HWND Window = nullptr; int64_t Area = 0; } search{ProcessId};
            EnumWindows([](HWND window, LPARAM argument) -> BOOL {
                auto& found = *reinterpret_cast<Search*>(argument);
                DWORD pid = 0;
                GetWindowThreadProcessId(window, &pid);
                RECT client{};
                if (pid == found.Pid && IsWindowVisible(window) && GetClientRect(window, &client)) {
                    const int64_t area = int64_t(client.right) * client.bottom;
                    if (area > found.Area) { found.Window = window; found.Area = area; }
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&search));
            Require(search.Window != nullptr, "No visible normal host window exists for capture");
            RECT client{};
            Require(GetClientRect(search.Window, &client), "Cannot inspect the host client area");
            const int width = client.right, height = client.bottom;
            Require(width > 0 && height > 0, "Host client area is empty");
            struct Surface {
                HWND Window; HDC Source = nullptr; HDC Memory = nullptr; HBITMAP Bitmap = nullptr; HGDIOBJ Previous = nullptr;
                ~Surface() {
                    if (Previous) SelectObject(Memory, Previous);
                    if (Bitmap) DeleteObject(Bitmap);
                    if (Memory) DeleteDC(Memory);
                    if (Source) ReleaseDC(Window, Source);
                }
            } surface{search.Window};
            surface.Source = GetDC(search.Window);
            surface.Memory = CreateCompatibleDC(surface.Source);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            void* pixels = nullptr;
            surface.Bitmap = CreateDIBSection(surface.Source, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            Require(surface.Source && surface.Memory && surface.Bitmap && pixels, "Cannot allocate the real window capture surface");
            surface.Previous = SelectObject(surface.Memory, surface.Bitmap);
            Require(BitBlt(surface.Memory, 0, 0, width, height, surface.Source, 0, 0, SRCCOPY | CAPTUREBLT), "Window capture failed");
            GdiFlush();
            const auto* bytes = static_cast<const uint8_t*>(pixels);
            const size_t byteCount = size_t(width) * size_t(height) * 4;
            uint64_t varied = 0;
            for (size_t index = 0; index < byteCount; index += 4) {
                if ((bytes[index] || bytes[index + 1] || bytes[index + 2]) &&
                    (bytes[index] != bytes[0] || bytes[index + 1] != bytes[1] || bytes[index + 2] != bytes[2])) ++varied;
            }
            Require(varied > 0, "Presented host window capture is blank or uniform");
            const auto path = Report.parent_path() / (Report.stem().string() + "-window.bmp");
            BITMAPFILEHEADER file{};
            file.bfType = 0x4d42;
            file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
            file.bfSize = static_cast<DWORD>(file.bfOffBits + byteCount);
            std::ofstream output(path, std::ios::binary);
            output.write(reinterpret_cast<const char*>(&file), sizeof(file));
            output.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
            output.write(static_cast<const char*>(pixels), static_cast<std::streamsize>(byteCount));
            output.close();
            Require(static_cast<bool>(output), "Cannot persist the window capture");
            Captures.push_back({path, HashFile(path), varied, static_cast<uint32_t>(width), static_cast<uint32_t>(height)});
        }
        void Write(bool success, int exitCode) const {
            Serialization::JsonSerializationBackend json;
            auto orderedIntervals = FrameIntervalsMs;
            std::sort(orderedIntervals.begin(), orderedIntervals.end());
            const auto percentile = [&](size_t numerator, size_t denominator) {
                if (orderedIntervals.empty()) return 0.0;
                const size_t rank = std::max<size_t>(1, (orderedIntervals.size() * numerator + denominator - 1) / denominator);
                return orderedIntervals[rank - 1];
            };
            json.Serialize("schema_version", uint32_t{1});
            json.Serialize("host", Host);
            json.Serialize("success", success);
            json.Serialize("exit_code", int32_t{exitCode});
            json.Serialize("process_id", ProcessId);
            json.Serialize("frames_rendered", GuiFrames);
            json.Serialize("gui_frames", GuiFrames);
            json.Serialize("scene_frames_rendered", RenderFrames);
            json.Serialize("update_count", Updates);
            json.Serialize("frame_interval_scope", "consecutive real host OnUpdate starts, including prior update, GUI and Present");
            json.Serialize("frame_interval_sample_count", static_cast<uint64_t>(FrameIntervalsMs.size()));
            json.Serialize("frame_interval_median_ms", percentile(1, 2));
            json.Serialize("frame_interval_p95_ms", percentile(95, 100));
            json.Serialize("window_event_count", WindowEvents);
            json.Serialize("event_count", static_cast<uint64_t>(Events.size()));
            json.Serialize("main_thread_id", MainThreadId);
            json.Serialize("graphics_thread_id", GraphicsThreadId);
            json.Serialize("graphics_calls_on_worker", WorkerGraphicsCalls);
            json.Serialize("graphics_thread_observation_scope", "host render and GUI dispatch");
            json.Serialize("error", Error);
            json.Serialize("child_process_id", ChildProcessId);
            json.Serialize("child_executable", ChildExecutable.generic_string());
            json.Serialize("child_executable_sha256", ChildExecutableSha256);
            json.Serialize("child_report", ChildReport.generic_string());
            json.BeginArray("frame_interval_ms");
            for (size_t index = 0; index < FrameIntervalsMs.size(); ++index) {
                json.BeginArrayElement(index); json.Serialize("", FrameIntervalsMs[index]); json.EndArrayElement();
            }
            json.EndArray();
            json.BeginArray("events");
            for (size_t index = 0; index < Events.size(); ++index) {
                json.BeginArrayElement(index); json.Serialize("", Events[index]); json.EndArrayElement();
            }
            json.EndArray();
            json.BeginArray("captures");
            for (size_t index = 0; index < Captures.size(); ++index) {
                json.BeginArrayElement(index); json.BeginObject("");
                json.Serialize("path", Captures[index].Path.generic_string());
                json.Serialize("sha256", Captures[index].Sha256);
                json.Serialize("drawn_pixels", Captures[index].DrawnPixels);
                json.Serialize("width", Captures[index].Width); json.Serialize("height", Captures[index].Height);
                json.EndObject(); json.EndArrayElement();
            }
            json.EndArray();
            json.BeginObject("snapshots");
            for (const auto& [key, value] : Snapshots) json.Serialize(key, value);
            json.EndObject();
            std::filesystem::create_directories(Report.parent_path());
            std::ofstream output(Report, std::ios::binary | std::ios::trunc);
            output << json.SaveToString();
            output.close();
            Require(static_cast<bool>(output), "Cannot persist the host report");
        }
    };

    inline int Run(CommandLineArguments args, std::string host,
        const std::function<std::unique_ptr<Application>(CommandLineArguments, std::shared_ptr<Session>)>& create) {
        bool requested = false;
        for (int index = 1; index < args.Count; ++index) {
            if (args[index] && std::string_view(args[index]) == "--ecs-smoke") requested = true;
        }
        if (!requested) {
            Log::Init();
            auto app = create(args, {}); app->Start(); app->Run(); return 0;
        }
        auto smoke = std::make_shared<Session>();
        smoke->Host = std::move(host);
        ScopedEnvironment environment;
        std::unique_ptr<Application> app;
        try {
            // The smoke-only path uses Windows Unicode argv without changing normal startup parsing.
            struct Arguments {
                int Count = 0;
                wchar_t** Values = CommandLineToArgvW(GetCommandLineW(), &Count);
                ~Arguments() { if (Values) LocalFree(Values); }
            } arguments;
            Require(arguments.Values != nullptr, "Cannot read the Unicode smoke arguments");
            for (int index = 1; index < arguments.Count; ++index) {
                const std::wstring_view argument(arguments.Values[index]);
                if (argument == L"--ecs-smoke") {
                    Require(smoke->Report.empty() && index + 1 < arguments.Count, "--ecs-smoke requires exactly one report path");
                    smoke->Report = std::filesystem::absolute(arguments.Values[++index]).lexically_normal();
                } else if (argument == L"--project") {
                    Require(index + 1 < arguments.Count, "--project requires a source path");
                    smoke->SourceProject = std::filesystem::absolute(arguments.Values[++index]).lexically_normal();
                }
            }
            Require(!smoke->Report.empty(), "The smoke report path is missing");
            smoke->PrepareWorkspace();
            environment.Isolate(smoke->Workspace);
            Log::Init();
            app = create(args, smoke);
            app->Start();
            app->Run();
            Require(smoke->WorkflowComplete && smoke->GuiFrames >= 3 && !smoke->Captures.empty() &&
                smoke->WorkerGraphicsCalls == 0, "The host exited before its smoke workflow completed");
            app.reset();
            smoke->Event("shutdown_clean");
            smoke->Write(true, 0);
            return 0;
        } catch (const std::exception& exception) {
            smoke->Error = exception.what();
        } catch (...) {
            smoke->Error = "The host threw a non-standard exception";
        }
        app.reset();
        try { smoke->Write(false, 1); } catch (...) {}
        return 1;
    }
}
