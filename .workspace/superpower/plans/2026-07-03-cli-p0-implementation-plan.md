# HuaEngine CLI P0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 固化 HuaEngine CLI 自动化契约，引入轻量 command catalog，并把 `CLICommandRunner.cpp` 拆成可维护的命令模块。

**Architecture:** `CommandRunner` 保留为总入口；`CLICommandCatalog` 提供命令契约；`CLIOptionParser` 统一解析和 usage error；`CLICommandContext` 承载路径解析和 response helper；各 domain handler 只编排对应命令并调用 `ApplicationOperations`。

**Tech Stack:** C++20, CMake, Windows smoke executables, HuaEngine `ApplicationOperations`, `ResultEnvelope`, `OperationRegistry`。

---

## File Structure

- Create: `Tests/CLIContractSmoke.cpp`
  - 外部进程契约测试，验证 exit code、JSON 结构、usage/manual intervention/cwd resolve。
- Modify: `CMakeLists.txt`
  - 新增 `CLIContractSmoke` target，依赖 `HuaEngineCLI`，并复制 CLI host 到 smoke 输出目录。
- Create: `CLI/src/CLICommandCatalog.h`
- Create: `CLI/src/CLICommandCatalog.cpp`
  - 集中声明所有当前 CLI commands、options、required options、formal operation。
- Create: `CLI/src/CLIOptionParser.h`
- Create: `CLI/src/CLIOptionParser.cpp`
  - 基于 catalog option 定义解析 tokens，返回 parsed options 或 `cli.usage`。
- Create: `CLI/src/CLICommandContext.h`
- Create: `CLI/src/CLICommandContext.cpp`
  - 抽出 normalize path、project resolve、scene path resolve、payload/details merge、usage/host failure helper。
- Create: `CLI/src/CLIMetaCommands.h`
- Create: `CLI/src/CLIMetaCommands.cpp`
- Create: `CLI/src/CLIProjectCommands.h`
- Create: `CLI/src/CLIProjectCommands.cpp`
- Create: `CLI/src/CLISceneCommands.h`
- Create: `CLI/src/CLISceneCommands.cpp`
- Create: `CLI/src/CLIAssetCommands.h`
- Create: `CLI/src/CLIAssetCommands.cpp`
- Create: `CLI/src/CLIScriptCommands.h`
- Create: `CLI/src/CLIScriptCommands.cpp`
- Create: `CLI/src/CLIValidationCommands.h`
- Create: `CLI/src/CLIValidationCommands.cpp`
  - 各 domain handler 迁移当前 `CLICommandRunner.cpp` 中的业务命令分支。
- Modify: `CLI/src/CLICommandRunner.h`
- Modify: `CLI/src/CLICommandRunner.cpp`
  - 改为 match catalog、parse options、dispatch handler。
- Modify: `CLI/src/CLIJsonWriter.cpp`
  - 如有必要，保证失败响应也稳定输出空 `payload` 和 `details`。
- Modify: `CLI/CMakeLists.txt`
  - 把新增 CLI core 源文件加入 `HuaEngineCLICore`。
- Modify: `Tests/CLIHostSmoke.cpp`
  - 增加 catalog 完整性、operation mapping、help-from-catalog 断言。
- Modify: `Tests/CLIWorkflowSmoke.cpp`
  - 增加 cwd project resolve 流程。
- Modify: `Docs/huaengine-cli.md`
  - 说明 command catalog、help/usage、`ops list` 与 CLI command list 的边界。

---

### Task 1: Add CLI Contract Smoke

**Files:**
- Create: `Tests/CLIContractSmoke.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing external process contract smoke**

Create `Tests/CLIContractSmoke.cpp` with this structure:

```cpp
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {
	struct ProcessResult {
		DWORD ExitCode = 0;
		std::string Output;
	};

	void Expect(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << message << std::endl;
			std::exit(1);
		}
	}

	std::wstring Utf8ToWide(std::string_view value);
	std::wstring QuoteForCommandLine(const std::wstring& argument);
	std::wstring BuildCommandLine(const std::filesystem::path& executable, const std::vector<std::string>& arguments);
	ProcessResult RunCLICommand(const std::filesystem::path& executable, const std::vector<std::string>& arguments, const std::filesystem::path& workingDirectory);
	std::filesystem::path GetCurrentExecutablePath();

	void ExpectContains(std::string_view output, std::string_view fragment, std::string_view context) {
		Expect(output.find(fragment) != std::string_view::npos, std::string(context) + ": missing fragment " + std::string(fragment));
	}

	void ExpectJsonEnvelope(const ProcessResult& result, std::string_view context) {
		ExpectContains(result.Output, "\"host\":\"huaengine-cli\"", context);
		ExpectContains(result.Output, "\"result\":{", context);
		ExpectContains(result.Output, "\"status\":\"", context);
		ExpectContains(result.Output, "\"payload\":{", context);
		ExpectContains(result.Output, "\"details\":[", context);
	}
}

int main() {
	const auto binaryDirectory = GetCurrentExecutablePath().parent_path();
	const auto cliExecutable = binaryDirectory / "HuaEngineCLI.exe";
	Expect(std::filesystem::exists(cliExecutable), "HuaEngineCLI.exe must exist next to CLIContractSmoke.exe");

	const auto tempRoot = std::filesystem::temp_directory_path() / "huaengine_cli_contract_smoke";
	std::error_code errorCode;
	std::filesystem::remove_all(tempRoot, errorCode);
	std::filesystem::create_directories(tempRoot, errorCode);
	Expect(!errorCode, "Failed to prepare temporary contract smoke directory");

	const std::vector<std::pair<std::string, std::vector<std::string>>> usageCases = {
		{ "empty arguments", {} },
		{ "unknown command", { "unknown" } },
		{ "unknown option", { "project", "status", "--bad-option" } },
		{ "missing option value", { "project", "status", "--path" } },
		{ "missing required option", { "scene", "create", "--project", tempRoot.string() } },
		{ "include scripts requires scene", { "validation", "run", "--path", tempRoot.string(), "--include-scripts" } }
	};

	for (const auto& [name, arguments] : usageCases) {
		const auto result = RunCLICommand(cliExecutable, arguments, binaryDirectory);
		Expect(result.ExitCode == 1, name + " should exit with code 1\n" + result.Output);
		ExpectJsonEnvelope(result, name);
		ExpectContains(result.Output, "\"operation\":\"cli.usage\"", name);
		ExpectContains(result.Output, "\"status\":\"failure\"", name);
	}

	const auto projectRoot = tempRoot / "ValidProject";
	auto initResult = RunCLICommand(cliExecutable, { "project", "init", "--root", projectRoot.string(), "--name", "ContractSmoke" }, binaryDirectory);
	Expect(initResult.ExitCode == 0, "project init should succeed\n" + initResult.Output);

	const auto nestedDirectory = projectRoot / "Scenes" / "Nested";
	std::filesystem::create_directories(nestedDirectory, errorCode);
	Expect(!errorCode, "Failed to create nested project directory");
	const auto cwdResult = RunCLICommand(cliExecutable, { "project", "status" }, nestedDirectory);
	Expect(cwdResult.ExitCode == 0, "project status should resolve cwd project context\n" + cwdResult.Output);
	ExpectJsonEnvelope(cwdResult, "cwd project status");
	ExpectContains(cwdResult.Output, "\"operation\":\"project.status\"", "cwd project status");

	const auto brokenProjectRoot = tempRoot / "BrokenProject";
	std::filesystem::create_directories(brokenProjectRoot / ".huaengine", errorCode);
	Expect(!errorCode, "Failed to create broken project metadata directory");
	std::ofstream(brokenProjectRoot / ".huaengine" / "project.json") << "{ invalid json";
	const auto manualResult = RunCLICommand(cliExecutable, { "project", "status", "--path", brokenProjectRoot.string() }, binaryDirectory);
	Expect(manualResult.ExitCode == 2, "broken project metadata should exit with code 2\n" + manualResult.Output);
	ExpectJsonEnvelope(manualResult, "manual intervention");
	ExpectContains(manualResult.Output, "\"status\":\"manual_intervention_required\"", "manual intervention");

	std::filesystem::remove_all(tempRoot, errorCode);
	std::cout << "CLIContractSmoke passed" << std::endl;
	return 0;
}
```

Copy the helper implementations from `Tests/CLIWorkflowSmoke.cpp` for `Utf8ToWide`, `QuoteForCommandLine`, `BuildCommandLine`, `RunCLICommand`, and `GetCurrentExecutablePath`. Keep comments in English if new comments are needed.

- [ ] **Step 2: Add the smoke target**

In root `CMakeLists.txt`, add after `CLIWorkflowSmoke`:

```cmake
add_executable(CLIContractSmoke Tests/CLIContractSmoke.cpp)
add_dependencies(CLIContractSmoke HuaEngineCLI)
if(WIN32)
    if(MSVC)
        set_target_properties(CLIContractSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(CLIContractSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(CLIContractSmoke)
copy_cli_host_to_smoke(CLIContractSmoke)
```

Also add folder property near the existing smoke folder properties:

```cmake
set_property(TARGET CLIContractSmoke PROPERTY FOLDER "Tests")
```

- [ ] **Step 3: Run contract smoke and verify it fails before implementation**

Run:

```powershell
cmake --build build --config Debug --target CLIContractSmoke
& .\build\bin\Debug-Windows-x64\smoke\CLIContractSmoke.exe
```

Expected before later tasks: build may pass, but smoke should fail on at least one current contract gap, such as required option validation or cwd/manual intervention coverage.

- [ ] **Step 4: Commit contract smoke**

```powershell
git add CMakeLists.txt Tests/CLIContractSmoke.cpp
git commit -m "test(cli): add contract smoke"
```

---

### Task 2: Add Catalog, Option Parser, and Command Context

**Files:**
- Create: `CLI/src/CLICommandCatalog.h`
- Create: `CLI/src/CLICommandCatalog.cpp`
- Create: `CLI/src/CLIOptionParser.h`
- Create: `CLI/src/CLIOptionParser.cpp`
- Create: `CLI/src/CLICommandContext.h`
- Create: `CLI/src/CLICommandContext.cpp`
- Modify: `CLI/CMakeLists.txt`

- [ ] **Step 1: Add catalog types and API**

Create `CLI/src/CLICommandCatalog.h`:

```cpp
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace HE::CLI {
	enum class CLICommandDomain {
		Meta,
		Project,
		Scene,
		Asset,
		Script,
		Validation
	};

	struct CLIOptionDefinition {
		std::string Name;
		bool RequiresValue = false;
		bool Required = false;
	};

	struct CLICommandDefinition {
		std::vector<std::string> Path;
		CLICommandDomain Domain = CLICommandDomain::Meta;
		std::string FormalOperation;
		std::string Summary;
		std::string Usage;
		std::vector<CLIOptionDefinition> Options;
	};

	struct CLICommandMatch {
		const CLICommandDefinition* Command = nullptr;
		size_t ConsumedArguments = 0;
	};

	class CLICommandCatalog {
	public:
		CLICommandCatalog();

		[[nodiscard]] const std::vector<CLICommandDefinition>& Commands() const { return m_Commands; }
		[[nodiscard]] std::optional<CLICommandMatch> Match(std::span<const std::string> arguments) const;
		[[nodiscard]] const CLICommandDefinition* Find(std::span<const std::string> path) const;

	private:
		void Register(CLICommandDefinition definition);

		std::vector<CLICommandDefinition> m_Commands;
	};
}
```

- [ ] **Step 2: Register current CLI commands**

Create `CLI/src/CLICommandCatalog.cpp` with all current commands:

```cpp
#include "enginepch.h"
#include "CLICommandCatalog.h"

#include <algorithm>

namespace HE::CLI {
	namespace {
		CLIOptionDefinition Value(std::string name, bool required = false) {
			return { std::move(name), true, required };
		}

		CLIOptionDefinition Flag(std::string name) {
			return { std::move(name), false, false };
		}

		bool PathEquals(std::span<const std::string> left, const std::vector<std::string>& right) {
			return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
		}
	}

	CLICommandCatalog::CLICommandCatalog() {
		Register({ { "help" }, CLICommandDomain::Meta, "cli.help", "Show CLI command help", "help", {} });
		Register({ { "ops", "list" }, CLICommandDomain::Meta, "cli.ops_list", "List formal application operations", "ops list", {} });

		Register({ { "project", "init" }, CLICommandDomain::Project, "project.initialize", "Initialize a HuaEngine project root", "project init [--root <path>] [--name <name>]", { Value("--root"), Value("--name") } });
		Register({ { "project", "status" }, CLICommandDomain::Project, "project.status", "Check project status", "project status [--path <path>]", { Value("--path") } });

		Register({ { "scene", "create" }, CLICommandDomain::Scene, "scene.create", "Create and save a scene", "scene create [--project <path>] --name <name> [--output <path>]", { Value("--project"), Value("--name", true), Value("--output") } });
		Register({ { "scene", "validate" }, CLICommandDomain::Scene, "scene.validate", "Validate a scene", "scene validate [--project <path>] --scene <path>", { Value("--project"), Value("--scene", true) } });
		Register({ { "scene", "entity", "create" }, CLICommandDomain::Scene, "scene.entity.create", "Create and save a scene entity", "scene entity create [--project <path>] --scene <path> --name <name> [--output <path>]", { Value("--project"), Value("--scene", true), Value("--name", true), Value("--output") } });
		Register({ { "scene", "entity", "delete" }, CLICommandDomain::Scene, "scene.entity.delete", "Delete and save a scene entity", "scene entity delete [--project <path>] --scene <path> --entity-id <id> [--output <path>]", { Value("--project"), Value("--scene", true), Value("--entity-id", true), Value("--output") } });
		Register({ { "scene", "component", "add" }, CLICommandDomain::Scene, "scene.component.add", "Add and save a scene component", "scene component add [--project <path>] --scene <path> --entity-id <id> --component <camera|mesh|material> [--output <path>]", { Value("--project"), Value("--scene", true), Value("--entity-id", true), Value("--component", true), Value("--output") } });
		Register({ { "scene", "component", "remove" }, CLICommandDomain::Scene, "scene.component.remove", "Remove and save a scene component", "scene component remove [--project <path>] --scene <path> --entity-id <id> --component <camera|mesh|material> [--output <path>]", { Value("--project"), Value("--scene", true), Value("--entity-id", true), Value("--component", true), Value("--output") } });

		Register({ { "asset", "register-default-mesh" }, CLICommandDomain::Asset, "asset.create_builtin_mesh", "Create and register a built-in mesh asset", "asset register-default-mesh [--project <path>] --asset-id <id> [--primitive <quad|cube|sphere>] [--name <name>]", { Value("--project"), Value("--asset-id", true), Value("--primitive"), Value("--name") } });
		Register({ { "asset", "validate" }, CLICommandDomain::Asset, "asset.validate", "Validate project assets", "asset validate [--path <path>]", { Value("--path") } });

		Register({ { "script", "status" }, CLICommandDomain::Script, "script.status", "Inspect script binding health", "script status [--project <path>] --scene <path>", { Value("--project"), Value("--scene", true) } });
		Register({ { "script", "initialize" }, CLICommandDomain::Script, "script.initialize", "Initialize scene scripts", "script initialize [--project <path>] --scene <path>", { Value("--project"), Value("--scene", true) } });
		Register({ { "script", "update" }, CLICommandDomain::Script, "script.update", "Update scene scripts", "script update [--project <path>] --scene <path>", { Value("--project"), Value("--scene", true) } });
		Register({ { "script", "shutdown" }, CLICommandDomain::Script, "script.shutdown", "Shutdown scene scripts", "script shutdown [--project <path>] --scene <path>", { Value("--project"), Value("--scene", true) } });

		Register({ { "validation", "run" }, CLICommandDomain::Validation, "validation.validate", "Run aggregate validation", "validation run [--path <path>] [--scene <path>] [--include-assets] [--include-scripts]", { Value("--path"), Value("--scene"), Flag("--include-assets"), Flag("--include-scripts") } });
	}

	void CLICommandCatalog::Register(CLICommandDefinition definition) {
		m_Commands.emplace_back(std::move(definition));
	}

	std::optional<CLICommandMatch> CLICommandCatalog::Match(std::span<const std::string> arguments) const {
		const CLICommandDefinition* best = nullptr;
		size_t bestSize = 0;
		for (const auto& command : m_Commands) {
			if (command.Path.size() > arguments.size() || command.Path.size() < bestSize) {
				continue;
			}
			if (std::equal(command.Path.begin(), command.Path.end(), arguments.begin())) {
				best = &command;
				bestSize = command.Path.size();
			}
		}
		if (!best) {
			return std::nullopt;
		}
		return CLICommandMatch{ best, bestSize };
	}

	const CLICommandDefinition* CLICommandCatalog::Find(std::span<const std::string> path) const {
		for (const auto& command : m_Commands) {
			if (PathEquals(path, command.Path)) {
				return &command;
			}
		}
		return nullptr;
	}
}
```

- [ ] **Step 3: Add parser API and implementation**

Create `CLI/src/CLIOptionParser.h`:

```cpp
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "CLICommandCatalog.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE::CLI {
	class CLIParsedOptions {
	public:
		[[nodiscard]] bool HasFlag(std::string_view key) const;
		[[nodiscard]] std::optional<std::string> GetValue(std::string_view key) const;

	private:
		friend class CLIOptionParser;
		std::unordered_map<std::string, std::string> m_Values;
		std::unordered_set<std::string> m_Flags;
	};

	class CLIOptionParser {
	public:
		[[nodiscard]] bool Parse(
			const CLICommandDefinition& command,
			std::span<const std::string> tokens,
			CLIParsedOptions& outOptions,
			ResultEnvelope& outError) const;
	};
}
```

Create `CLI/src/CLIOptionParser.cpp` using `MakeUsageError` from `CLICommandContext`:

```cpp
#include "enginepch.h"
#include "CLIOptionParser.h"

#include "CLICommandContext.h"

namespace HE::CLI {
	bool CLIParsedOptions::HasFlag(std::string_view key) const {
		return m_Flags.contains(std::string(key));
	}

	std::optional<std::string> CLIParsedOptions::GetValue(std::string_view key) const {
		const auto existing = m_Values.find(std::string(key));
		if (existing == m_Values.end()) {
			return std::nullopt;
		}
		return existing->second;
	}

	bool CLIOptionParser::Parse(
		const CLICommandDefinition& command,
		std::span<const std::string> tokens,
		CLIParsedOptions& outOptions,
		ResultEnvelope& outError) const {
		for (size_t index = 0; index < tokens.size(); ++index) {
			const auto& token = tokens[index];
			if (!token.starts_with("--")) {
				outError = MakeUsageError(command, "Unexpected positional argument", token);
				return false;
			}

			const auto option = std::find_if(command.Options.begin(), command.Options.end(), [&](const auto& definition) {
				return definition.Name == token;
			});
			if (option == command.Options.end()) {
				outError = MakeUsageError(command, "Unknown option", token);
				return false;
			}

			if (!option->RequiresValue) {
				outOptions.m_Flags.insert(token);
				continue;
			}

			if (index + 1 >= tokens.size() || tokens[index + 1].starts_with("--")) {
				outError = MakeUsageError(command, "Option requires a value", token);
				return false;
			}

			outOptions.m_Values[token] = tokens[++index];
		}

		for (const auto& option : command.Options) {
			if (option.Required && !outOptions.GetValue(option.Name).has_value()) {
				outError = MakeUsageError(command, "Required option is missing", option.Name);
				return false;
			}
		}

		return true;
	}
}
```

- [ ] **Step 4: Add command context helpers**

Create `CLI/src/CLICommandContext.h`:

```cpp
#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "CLICommandCatalog.h"
#include "CLIOptionParser.h"
#include "CLICommandRunner.h"
#include "HuaEngine/Application/ApplicationOperations.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE::CLI {
	struct CLICommandContext {
		ApplicationOperations& Operations;
		std::filesystem::path WorkingDirectory;
		const CLICommandCatalog& Catalog;
	};

	[[nodiscard]] std::filesystem::path NormalizePath(const std::filesystem::path& path);
	[[nodiscard]] ResultEnvelope MakeUsageError(const CLICommandDefinition& command, std::string_view summary, std::string_view context = {});
	[[nodiscard]] ResultEnvelope MakeUsageError(std::string_view summary, std::string_view context = {});
	[[nodiscard]] ResultEnvelope MakeHostFailure(std::string_view operation, std::string_view summary, std::string_view context = {});
	void MergeDetails(ResultEnvelope& destination, const ResultEnvelope& source);
	void CopyPayloadIfMissing(ResultEnvelope& destination, const ResultEnvelope& source);
	[[nodiscard]] bool ResolveProjectContext(CLICommandContext& context, const std::optional<std::string>& explicitPath, ProjectContext& outContext, ResultEnvelope& outError);
	[[nodiscard]] std::filesystem::path ResolveProjectRelativePath(const std::filesystem::path& inputPath, const std::filesystem::path& rootPath);
	[[nodiscard]] std::filesystem::path ResolveScenePath(const std::string& sceneArgument, const std::optional<ProjectContext>& projectContext, const std::filesystem::path& workingDirectory);
}
```

Create `CLI/src/CLICommandContext.cpp` by moving the equivalent helper functions from the current anonymous namespace in `CLICommandRunner.cpp`. `MakeUsageError(const CLICommandDefinition&)` must add a `cli.usage.command` detail with `command.Usage`:

```cpp
result.AddDetail({ DiagnosticSeverity::Info, "cli.usage.command", command.Usage, {} });
```

- [ ] **Step 5: Update `CLI/CMakeLists.txt`**

Add new core sources and headers:

```cmake
set(CLI_CORE_SOURCES
    src/CLIApplication.cpp
    src/CLICommandCatalog.cpp
    src/CLICommandContext.cpp
    src/CLICommandRunner.cpp
    src/CLIJsonWriter.cpp
    src/CLIOptionParser.cpp
)

set(CLI_CORE_HEADERS
    src/CLIApplication.h
    src/CLICommandCatalog.h
    src/CLICommandContext.h
    src/CLICommandRunner.h
    src/CLIJsonWriter.h
    src/CLIOptionParser.h
)
```

- [ ] **Step 6: Build core**

Run:

```powershell
cmake --build build --config Debug --target HuaEngineCLI
```

Expected: compile may fail until Task 3 wires the runner if moved helpers are referenced incorrectly; fix only compile errors in the files created in this task.

- [ ] **Step 7: Commit catalog/parser/context**

```powershell
git add CLI/CMakeLists.txt CLI/src/CLICommandCatalog.* CLI/src/CLICommandContext.* CLI/src/CLIOptionParser.*
git commit -m "refactor(cli): add command catalog and parser"
```

---

### Task 3: Split Runner Into Domain Handlers

**Files:**
- Create/Modify all `CLI/src/CLI*Commands.*`
- Modify: `CLI/src/CLICommandRunner.cpp`
- Modify: `CLI/src/CLICommandRunner.h`
- Modify: `CLI/CMakeLists.txt`

- [ ] **Step 1: Add handler declarations**

Create one header per domain. Example `CLI/src/CLIProjectCommands.h`:

```cpp
#pragma once

#include "CLICommandContext.h"

namespace HE::CLI {
	[[nodiscard]] CLICommandResponse RunProjectCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions& options,
		CLICommandContext& context);
}
```

Use the same shape for:

```cpp
RunMetaCommand
RunSceneCommand
RunAssetCommand
RunScriptCommand
RunValidationCommand
```

- [ ] **Step 2: Move meta commands**

Create `CLI/src/CLIMetaCommands.cpp`. Implement:

```cpp
CLICommandResponse RunMetaCommand(
	const CLICommandDefinition& command,
	const CLIParsedOptions& options,
	CLICommandContext& context) {
	if (command.Path == std::vector<std::string>{ "help" }) {
		auto result = ResultEnvelope::Success("cli.help", "command_line", "CLI command help");
		for (const auto& entry : context.Catalog.Commands()) {
			result.AddDetail({ DiagnosticSeverity::Info, "cli.help.command", entry.Usage, entry.Summary });
		}
		return { std::move(result) };
	}

	if (command.Path == std::vector<std::string>{ "ops", "list" }) {
		auto result = ResultEnvelope::Success("cli.ops_list", "operation_registry", "Formal operation registry listed");
		result.SetPayloadValue("operation_count", std::to_string(context.Operations.GetOperationRegistry().Size()));
		return { std::move(result), context.Operations.GetOperationRegistry().List() };
	}

	return { MakeUsageError(command, "Unsupported meta command") };
}
```

This ensures help is generated from catalog.

- [ ] **Step 3: Move project commands**

Create `CLI/src/CLIProjectCommands.cpp` by moving the current `project init` and `project status` branches. Use `CLIParsedOptions::GetValue` and `CLICommandContext` helpers. Preserve operation behavior and payloads.

The required shape:

```cpp
if (command.Path == std::vector<std::string>{ "project", "init" }) {
	ProjectContext projectContext;
	auto result = context.Operations.InitializeProject(
		options.GetValue("--root").value_or(NormalizePath(context.WorkingDirectory).string()),
		&projectContext,
		options.GetValue("--name").value_or(std::string()));
	return { std::move(result) };
}

if (command.Path == std::vector<std::string>{ "project", "status" }) {
	ProjectContext projectContext;
	ResultEnvelope resolveResult;
	if (!ResolveProjectContext(context, options.GetValue("--path"), projectContext, resolveResult)) {
		return { std::move(resolveResult) };
	}
	return { context.Operations.CheckProjectStatus(projectContext) };
}
```

- [ ] **Step 4: Move scene commands**

Create `CLI/src/CLISceneCommands.cpp` by moving the current scene branches:

- `scene create`
- `scene validate`
- `scene entity create`
- `scene entity delete`
- `scene component add`
- `scene component remove`

Also move these local parsers into the file:

```cpp
std::string SanitizeFileStem(std::string_view value);
std::optional<uint32_t> ParseEntityId(std::string_view value);
std::optional<SceneComponentKind> ParseSceneComponentKind(std::string_view value);
```

Do not add new scene commands in this task.

- [ ] **Step 5: Move asset commands**

Create `CLI/src/CLIAssetCommands.cpp` by moving:

- `asset register-default-mesh`
- `asset validate`

Move `ParseBuiltinMeshPrimitive` into this file.

- [ ] **Step 6: Move script commands**

Create `CLI/src/CLIScriptCommands.cpp` by moving:

- `script status`
- `script initialize`
- `script update`
- `script shutdown`

Preserve the existing behavior where non-status script commands attach runtime before operation execution.

- [ ] **Step 7: Move validation command**

Create `CLI/src/CLIValidationCommands.cpp` by moving `validation run`. Keep the explicit validation rule:

```cpp
if (options.HasFlag("--include-scripts") && !options.GetValue("--scene").has_value()) {
	return { MakeUsageError(command, "validation run with --include-scripts requires --scene") };
}
```

- [ ] **Step 8: Replace `CommandRunner::Run` with catalog dispatch**

Modify `CLI/src/CLICommandRunner.cpp` so it no longer contains domain business branches:

```cpp
CLICommandResponse CommandRunner::Run(
	const std::vector<std::string>& arguments,
	const std::filesystem::path& workingDirectory) const {
	if (!m_Operations) {
		return { MakeHostFailure("cli.host", "Application operations are not available") };
	}
	if (arguments.empty()) {
		return { MakeUsageError("No command specified") };
	}

	CLICommandCatalog catalog;
	const auto match = catalog.Match(arguments);
	if (!match.has_value()) {
		return { MakeUsageError("Unknown command", arguments.front()) };
	}

	std::span<const std::string> optionTokens(
		arguments.data() + match->ConsumedArguments,
		arguments.size() - match->ConsumedArguments);

	CLIParsedOptions options;
	ResultEnvelope optionError;
	CLIOptionParser parser;
	if (!parser.Parse(*match->Command, optionTokens, options, optionError)) {
		return { std::move(optionError) };
	}

	CLICommandContext context{ *m_Operations, workingDirectory, catalog };
	switch (match->Command->Domain) {
	case CLICommandDomain::Meta:
		return RunMetaCommand(*match->Command, options, context);
	case CLICommandDomain::Project:
		return RunProjectCommand(*match->Command, options, context);
	case CLICommandDomain::Scene:
		return RunSceneCommand(*match->Command, options, context);
	case CLICommandDomain::Asset:
		return RunAssetCommand(*match->Command, options, context);
	case CLICommandDomain::Script:
		return RunScriptCommand(*match->Command, options, context);
	case CLICommandDomain::Validation:
		return RunValidationCommand(*match->Command, options, context);
	}

	return { MakeUsageError(*match->Command, "Unsupported command domain") };
}
```

Include all handler headers and new parser/catalog/context headers.

- [ ] **Step 9: Update `CLI/CMakeLists.txt` with handlers**

Add handler `.cpp` files to `CLI_CORE_SOURCES` and handler `.h` files to `CLI_CORE_HEADERS`.

- [ ] **Step 10: Build and run CLI host smoke**

Run:

```powershell
cmake --build build --config Debug --target CLIHostSmoke
& .\build\bin\Debug-Windows-x64\smoke\CLIHostSmoke.exe
```

Expected: `CLIHostSmoke passed`.

- [ ] **Step 11: Commit runner split**

```powershell
git add CLI/CMakeLists.txt CLI/src/CLI*Commands.* CLI/src/CLICommandRunner.*
git commit -m "refactor(cli): split command runner by domain"
```

---

### Task 4: Strengthen Host and Workflow Smoke, Update CLI Docs

**Files:**
- Modify: `Tests/CLIHostSmoke.cpp`
- Modify: `Tests/CLIWorkflowSmoke.cpp`
- Modify: `Docs/huaengine-cli.md`

- [ ] **Step 1: Add catalog assertions to `CLIHostSmoke`**

Include `CLICommandCatalog.h`. Add after runner creation:

```cpp
HE::CLI::CLICommandCatalog catalog;
Expect(!catalog.Commands().empty(), "CLI command catalog should not be empty");
for (const auto& command : catalog.Commands()) {
	Expect(!command.Summary.empty(), "Catalog command summary should not be empty");
	Expect(!command.Usage.empty(), "Catalog command usage should not be empty");
	if (!command.FormalOperation.starts_with("cli.")) {
		Expect(application.GetOperations().Supports(command.FormalOperation), "Catalog formal operation should exist: " + command.FormalOperation);
	}
}
```

Run `help` and assert catalog-driven details:

```cpp
auto helpResponse = runner.Run({ "help" }, tempRoot);
Expect(helpResponse.Result.Succeeded(), "help should succeed");
Expect(!helpResponse.Result.Details.empty(), "help should expose catalog commands");
Expect(helpResponse.Result.Details[0].Code == "cli.help.command", "help details should be generated from catalog");
```

- [ ] **Step 2: Add cwd resolve flow to `CLIWorkflowSmoke`**

After project init creates project marker, create a nested directory:

```cpp
const std::filesystem::path nestedProjectDirectory = tempRoot / "Scenes" / "Nested";
std::filesystem::create_directories(nestedProjectDirectory, errorCode);
Expect(!errorCode, "Failed to create nested project directory");
```

After the main workflow loop, run:

```cpp
const auto cwdStatus = RunCLICommand(cliExecutable, { "project", "status" }, nestedProjectDirectory);
Expect(cwdStatus.ExitCode == 0, "project status should resolve project from cwd\n" + cwdStatus.Output);
ExpectContains(cwdStatus.Output, "\"operation\":\"project.status\"", "cwd project status");
ExpectContains(cwdStatus.Output, "\"status\":\"success\"", "cwd project status");
```

- [ ] **Step 3: Update `Docs/huaengine-cli.md`**

Add a short section after command overview:

```markdown
## Command Catalog 与 Operation Registry

CLI 内部通过 command catalog 维护命令契约。catalog 描述 CLI command path、usage、options 和映射的正式 operation，用于生成 help 和 usage 诊断。

`ops list` 不展示 CLI command catalog。它只展示 `ApplicationOperations` 注册的正式 operation registry，用于确认 GUI、CLI、Agent 共同消费的正式能力面。

因此：

- 想看当前正式能力面：使用 `ops list`
- 想看 CLI 命令用法：使用 `help`
- 新增 CLI 命令时，应先进入 command catalog，再实现 handler，并补 smoke
```

- [ ] **Step 4: Run host and workflow smoke**

```powershell
cmake --build build --config Debug --target CLIHostSmoke
cmake --build build --config Debug --target CLIWorkflowSmoke
& .\build\bin\Debug-Windows-x64\smoke\CLIHostSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\CLIWorkflowSmoke.exe
```

Expected: both pass.

- [ ] **Step 5: Commit smoke/docs updates**

```powershell
git add Tests/CLIHostSmoke.cpp Tests/CLIWorkflowSmoke.cpp Docs/huaengine-cli.md
git commit -m "test(cli): strengthen catalog and workflow coverage"
```

---

### Task 5: Full Verification and Cleanup

**Files:**
- Modify only if verification reveals small issues in files touched by Tasks 1-4.

- [ ] **Step 1: Run all required CLI verification targets**

```powershell
cmake --build build --config Debug --target HuaEngineCLI
cmake --build build --config Debug --target CLIContractSmoke
cmake --build build --config Debug --target CLIHostSmoke
cmake --build build --config Debug --target CLIWorkflowSmoke
& .\build\bin\Debug-Windows-x64\smoke\CLIContractSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\CLIHostSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\CLIWorkflowSmoke.exe
```

Expected:

```text
CLIContractSmoke passed
CLIHostSmoke passed
CLIWorkflowSmoke passed
```

- [ ] **Step 2: Run focused regression checks**

```powershell
cmake --build build --config Debug --target ApplicationOperationsSmoke
cmake --build build --config Debug --target AssetServiceSmoke
& .\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\AssetServiceSmoke.exe
```

Expected: both pass. These guard shared operations used by CLI.

- [ ] **Step 3: Inspect status and diffs**

```powershell
git status --short
git diff --stat HEAD
```

Expected: no uncommitted implementation changes except pre-existing untracked `.workspace/cli-p0-handoff.md` and `.workspace/engine_roadmap.md`.

- [ ] **Step 4: Commit any verification fixes**

If verification required fixes:

```powershell
git add <fixed-files>
git commit -m "fix(cli): address p0 verification issues"
```

If no fixes were required, do not create an empty commit.

---

## Self-Review Notes

- Spec coverage: tasks cover contract smoke, catalog, parser, context, runner split, host/workflow tests, docs, and verification.
- Operation exposure: intentionally limited to existing CLI commands; all-operation exposure remains out of scope.
- `ops list`: preserved as `OperationRegistry` output.
- No third-party CLI parser is introduced.
- All new documentation text is Chinese-first where user-facing.
