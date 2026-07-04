#include "enginepch.h"
#include "CLICommandCatalog.h"

#include <algorithm>
#include <initializer_list>

namespace HE::CLI {
	namespace {
		CLIOptionDefinition ValueOption(std::string name, std::string summary, bool required = false) {
			return { std::move(name), true, required, std::move(summary) };
		}

		CLIOptionDefinition FlagOption(std::string name, std::string summary) {
			return { std::move(name), false, false, std::move(summary) };
		}

		bool PathEquals(std::span<const std::string> left, std::span<const std::string_view> right) {
			if (left.size() != right.size()) {
				return false;
			}

			for (size_t index = 0; index < left.size(); ++index) {
				if (left[index] != right[index]) {
					return false;
				}
			}

			return true;
		}

		bool IsPrefix(std::span<const std::string> tokens, std::span<const std::string> path) {
			if (tokens.size() < path.size()) {
				return false;
			}

			for (size_t index = 0; index < path.size(); ++index) {
				if (tokens[index] != path[index]) {
					return false;
				}
			}

			return true;
		}
	}

	CLICommandCatalog::CLICommandCatalog() {
		Register({
			{ "help" },
			CLICommandDomain::CLI,
			"cli.help",
			"Show CLI command help.",
			"help",
			{}
		});
		Register({
			{ "ops", "list" },
			CLICommandDomain::Operations,
			"cli.ops_list",
			"List the formal operation registry.",
			"ops list",
			{}
		});
		Register({
			{ "project", "init" },
			CLICommandDomain::Project,
			"project.initialize",
			"Initialize a HuaEngine project root.",
			"project init [--root <path>] [--name <name>]",
			{
				ValueOption("--root", "Project root path."),
				ValueOption("--name", "Project display name.")
			}
		});
		Register({
			{ "project", "status" },
			CLICommandDomain::Project,
			"project.check_status",
			"Check project metadata and managed directories.",
			"project status [--path <path>]",
			{ ValueOption("--path", "Project path or child path.") }
		});
		Register({
			{ "scene", "create" },
			CLICommandDomain::Scene,
			"scene.create",
			"Create and save a scene.",
			"scene create --name <name> [--project <path>] [--output <scene>]",
			{
				ValueOption("--project", "Project path or child path."),
				ValueOption("--name", "Scene name.", true),
				ValueOption("--output", "Output scene path.")
			}
		});
		Register({
			{ "scene", "validate" },
			CLICommandDomain::Scene,
			"scene.validate",
			"Validate a scene file.",
			"scene validate --scene <scene> [--project <path>]",
			{
				ValueOption("--project", "Project path or child path."),
				ValueOption("--scene", "Scene path.", true)
			}
		});
		Register({
			{ "scene", "entity", "create" },
			CLICommandDomain::Scene,
			"scene.entity.create",
			"Create a scene entity and save the scene.",
			"scene entity create --scene <scene> --name <name> [--project <path>] [--output <scene>]",
			{
				ValueOption("--project", "Project path or child path."),
				ValueOption("--scene", "Scene path.", true),
				ValueOption("--name", "Entity name.", true),
				ValueOption("--output", "Output scene path.")
			}
		});
		Register({
			{ "scene", "entity", "delete" },
			CLICommandDomain::Scene,
			"scene.entity.delete",
			"Delete a scene entity and save the scene.",
			"scene entity delete --scene <scene> --entity-id <id> [--project <path>] [--output <scene>]",
			{
				ValueOption("--project", "Project path or child path."),
				ValueOption("--scene", "Scene path.", true),
				ValueOption("--entity-id", "Entity id.", true),
				ValueOption("--output", "Output scene path.")
			}
		});
		Register({
			{ "scene", "component", "add" },
			CLICommandDomain::Scene,
			"scene.component.add",
			"Add a supported component and save the scene.",
			"scene component add --scene <scene> --entity-id <id> --component <camera|mesh|material> [--project <path>] [--output <scene>]",
			{
				ValueOption("--project", "Project path or child path."),
				ValueOption("--scene", "Scene path.", true),
				ValueOption("--entity-id", "Entity id.", true),
				ValueOption("--component", "Component kind.", true),
				ValueOption("--output", "Output scene path.")
			}
		});
		Register({
			{ "scene", "component", "remove" },
			CLICommandDomain::Scene,
			"scene.component.remove",
			"Remove a supported component and save the scene.",
			"scene component remove --scene <scene> --entity-id <id> --component <camera|mesh|material> [--project <path>] [--output <scene>]",
			{
				ValueOption("--project", "Project path or child path."),
				ValueOption("--scene", "Scene path.", true),
				ValueOption("--entity-id", "Entity id.", true),
				ValueOption("--component", "Component kind.", true),
				ValueOption("--output", "Output scene path.")
			}
		});
		Register({
			{ "asset", "register-default-mesh" },
			CLICommandDomain::Asset,
			"asset.create_builtin_mesh",
			"Create and register a built-in mesh asset.",
			"asset register-default-mesh --asset-id <id> [--project <path>] [--primitive <quad|cube|sphere>] [--name <name>]",
			{
				ValueOption("--project", "Project path or child path."),
				ValueOption("--asset-id", "Asset id.", true),
				ValueOption("--primitive", "Built-in mesh primitive."),
				ValueOption("--name", "Mesh name.")
			}
		});
		Register({
			{ "asset", "manifest", "init" },
			CLICommandDomain::Asset,
			"asset.manifest.init",
			"Initialize project asset manifest.",
			"asset manifest init --project <path>",
			{ ValueOption("--project", "Project path or child path.") }
		});
		Register({
			{ "asset", "import" },
			CLICommandDomain::Asset,
			"asset.import",
			"Import a single asset file.",
			"asset import --project <path> --asset-id <path> --kind <mesh|material|texture2d>",
			{
				ValueOption("--project", "Project path or child path."),
				ValueOption("--asset-id", "Asset id.", true),
				ValueOption("--kind", "Asset kind.", true)
			}
		});
		Register({
			{ "asset", "list" },
			CLICommandDomain::Asset,
			"asset.list",
			"List project assets.",
			"asset list --project <path>",
			{ ValueOption("--project", "Project path or child path.") }
		});
		Register({
			{ "asset", "validate" },
			CLICommandDomain::Asset,
			"asset.validate",
			"Validate project asset registry health.",
			"asset validate [--path <path>]",
			{ ValueOption("--path", "Project path or child path.") }
		});

		Register({
			{ "reflection", "scan" },
			CLICommandDomain::Reflection,
			"reflection.scan",
			"Scan source reflection markers into a manifest.",
			"reflection scan --root <path> [--out <manifest>]",
			{
				ValueOption("--root", "Repository root path.", true),
				ValueOption("--out", "Manifest output path.")
			}
		});
		Register({
			{ "reflection", "generate" },
			CLICommandDomain::Reflection,
			"reflection.generate",
			"Generate C++ reflection metadata files.",
			"reflection generate --root <path> [--out-dir <path>] [--out <manifest>]",
			{
				ValueOption("--root", "Repository root path.", true),
				ValueOption("--out-dir", "Generated C++ output directory."),
				ValueOption("--out", "Manifest output path.")
			}
		});
		Register({
			{ "reflection", "validate" },
			CLICommandDomain::Reflection,
			"reflection.validate",
			"Validate source reflection markers.",
			"reflection validate --root <path>",
			{ ValueOption("--root", "Repository root path.", true) }
		});

		Register({
			{ "validation", "run" },
			CLICommandDomain::Validation,
			"validation.validate",
			"Run aggregate validation.",
			"validation run [--path <path>] [--scene <scene>] [--include-assets]",
			{
				ValueOption("--path", "Project path or child path."),
				ValueOption("--scene", "Scene path."),
				FlagOption("--include-assets", "Include asset validation.")
			}
		});
	}

	CLICommandMatch CLICommandCatalog::Match(std::span<const std::string> tokens) const {
		CLICommandMatch bestMatch;
		for (const auto& command : m_Commands) {
			if (!IsPrefix(tokens, command.Path)) {
				continue;
			}

			if (command.Path.size() > bestMatch.MatchedTokenCount) {
				bestMatch.Command = &command;
				bestMatch.MatchedTokenCount = command.Path.size();
			}
		}

		return bestMatch;
	}

	const CLICommandDefinition* CLICommandCatalog::Find(std::span<const std::string_view> path) const {
		const auto command = std::find_if(m_Commands.begin(), m_Commands.end(), [path](const CLICommandDefinition& candidate) {
			return PathEquals(candidate.Path, path);
		});

		return command == m_Commands.end() ? nullptr : &(*command);
	}

	const CLICommandDefinition* CLICommandCatalog::Find(std::initializer_list<std::string_view> path) const {
		return Find(std::span<const std::string_view>(path.begin(), path.size()));
	}

	void CLICommandCatalog::Register(CLICommandDefinition command) {
		m_Commands.push_back(std::move(command));
	}
}
