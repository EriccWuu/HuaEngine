#include "enginepch.h"
#include "AssetImportService.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "HuaEngine/Asset/AssetSourcePath.h"
#include "HuaEngine/Asset/Import/AssetImportFingerprint.h"
#include "HuaEngine/Asset/Import/AssetSourceHash.h"
#include "HuaEngine/Asset/Metadata/AssetMeta.h"

namespace {
	struct ImportPlanNode {
		HE::AssetGuid Guid;
		const HE::AssetManifestRecord* Record = nullptr;
		const HE::AssetImporter* Importer = nullptr;
		std::filesystem::path SourcePath;
		std::vector<HE::AssetGuid> Dependencies;
		std::shared_ptr<const HE::AssetImportSettings> Settings;
		bool Force = false;
		bool SkipWithoutSource = false;
	};

	void AppendDetails(std::vector<HE::DiagnosticEntry>& output, const HE::ResultEnvelope& result) {
		output.insert(output.end(), result.Details.begin(), result.Details.end());
	}

	std::string DescribeCycleNode(const ImportPlanNode& node) {
		return node.Guid + " [" + node.SourcePath.generic_string() + "]";
	}

	HE::ResultEnvelope BuildImportPlan(
		const HE::ProjectContext& context,
		const HE::AssetManifest& manifest,
		const HE::AssetImporterRegistry& registry,
		HE::AssetLibrary& library,
		std::span<const HE::AssetGuid> requestedGuids,
		HE::AssetImportPolicy policy,
		std::vector<ImportPlanNode>& output,
		uint32_t& failedAssets,
		bool& builtinFailure) {
		output.clear();
		failedAssets = 0;
		builtinFailure = false;
		std::map<HE::AssetGuid, ImportPlanNode> nodes;
		std::vector<HE::DiagnosticEntry> diagnostics;
		std::set<HE::AssetGuid> invalidGuids;
		std::vector<HE::AssetGuid> roots(requestedGuids.begin(), requestedGuids.end());
		std::sort(roots.begin(), roots.end());
		roots.erase(std::unique(roots.begin(), roots.end()), roots.end());
		const std::set<HE::AssetGuid> forcedRoots = policy == HE::AssetImportPolicy::Force
			? std::set<HE::AssetGuid>(roots.begin(), roots.end())
			: std::set<HE::AssetGuid>{};

		std::function<void(const HE::AssetGuid&, bool)> discover;
		discover = [&](const HE::AssetGuid& guid, bool force) {
			if (guid.empty()) {
				invalidGuids.insert(guid);
				diagnostics.push_back({ HE::DiagnosticSeverity::Error, "asset.import.record_missing", "Asset dependency GUID is empty", {} });
				return;
			}
			force |= forcedRoots.contains(guid);
			auto [it, inserted] = nodes.try_emplace(guid);
			auto& node = it->second;
			node.Guid = guid;
			node.Force |= force;
			if (!inserted) return;

			node.Record = manifest.FindByGuid(guid);
			if (!node.Record || (node.Record->Source != HE::AssetSource::File && node.Record->Source != HE::AssetSource::Builtin)) {
				invalidGuids.insert(guid);
				diagnostics.push_back({ HE::DiagnosticSeverity::Error, "asset.import.record_missing", "Asset import requires a file-backed or builtin manifest record", guid });
				return;
			}

			auto sourceResult = HE::ResolveAssetSourcePath(context, *node.Record, node.SourcePath);
			if (!sourceResult.Succeeded()) {
				invalidGuids.insert(guid);
				AppendDetails(diagnostics, sourceResult);
				return;
			}
			node.Importer = registry.Find(node.Record->Kind, node.SourcePath.extension().string());
			if (!node.Importer) {
				invalidGuids.insert(guid);
				diagnostics.push_back({ HE::DiagnosticSeverity::Error, "asset.import.importer_missing", "No asset importer supports the manifest kind and source extension", node.Record->AssetId });
				return;
			}
			std::unique_ptr<HE::AssetImportSettings> settings;
			if (node.Record->Source == HE::AssetSource::File) {
				HE::AssetMeta meta;
				auto metaResult = HE::LoadAssetMeta(node.SourcePath, meta);
				if (!metaResult.Succeeded() || meta.ImporterId != node.Importer->GetId() || meta.SettingsVersion != node.Importer->GetSettingsVersion()) {
					invalidGuids.insert(guid);
					AppendDetails(diagnostics, metaResult);
					diagnostics.push_back({ HE::DiagnosticSeverity::Error, "asset.meta.importer_mismatch", "Asset metadata does not match the selected importer settings schema", node.SourcePath.generic_string() });
					return;
				}
				auto decodeResult = node.Importer->DecodeSettings(meta.Settings, settings);
				if (!decodeResult.Succeeded() || !settings || !node.Importer->ValidateSettings(*settings).Succeeded()) {
					invalidGuids.insert(guid);
					AppendDetails(diagnostics, decodeResult);
					return;
				}
			}
			else {
				settings = node.Importer->CreateDefaultSettings();
			}
			node.Settings = std::move(settings);

			std::error_code errorCode;
			if (!std::filesystem::is_regular_file(node.SourcePath, errorCode)) {
				if (!node.Force && library.IsArtifactAvailable(
					node.Record->Guid,
					node.Record->Kind,
					node.Importer->GetId(),
					node.Importer->GetVersion(),
					node.Importer->GetArtifactVersion())) {
					node.SkipWithoutSource = true;
					return;
				}
				invalidGuids.insert(guid);
				diagnostics.push_back({ HE::DiagnosticSeverity::Error, "asset.import.source_missing", "Asset source file is missing", node.SourcePath.generic_string() });
				return;
			}

			const HE::AssetImportContext importContext{
				.Project = context,
				.SourceAsset = *node.Record,
				.SourcePath = node.SourcePath,
				.Manifest = &manifest,
				.Library = &library,
				.Settings = node.Settings.get()
			};
			auto dependencyResult = node.Importer->CollectDependencies(importContext, node.Dependencies);
			if (!dependencyResult.Succeeded()) {
				invalidGuids.insert(guid);
				AppendDetails(diagnostics, dependencyResult);
				if (dependencyResult.Details.empty()) {
					diagnostics.push_back({ HE::DiagnosticSeverity::Error, "asset.import.dependencies_invalid", dependencyResult.Summary, node.SourcePath.generic_string() });
				}
				return;
			}
			std::sort(node.Dependencies.begin(), node.Dependencies.end());
			node.Dependencies.erase(std::unique(node.Dependencies.begin(), node.Dependencies.end()), node.Dependencies.end());
			for (const auto& dependency : node.Dependencies) discover(dependency, false);
		};

		for (const auto& guid : roots) discover(guid, policy == HE::AssetImportPolicy::Force);

		if (policy == HE::AssetImportPolicy::Force) {
			std::vector<HE::AssetGuid> pending = roots;
			std::set<HE::AssetGuid> visitedDependents(roots.begin(), roots.end());
			while (!pending.empty()) {
				auto guid = std::move(pending.back());
				pending.pop_back();
				for (const auto& dependent : library.FindDependents(guid)) {
					discover(dependent, false);
					if (visitedDependents.insert(dependent).second) pending.push_back(dependent);
				}
			}
		}

		bool propagatedFailure = true;
		while (propagatedFailure) {
			propagatedFailure = false;
			for (const auto& [guid, node] : nodes) {
				if (invalidGuids.contains(guid)) continue;
				const auto failedDependency = std::find_if(node.Dependencies.begin(), node.Dependencies.end(), [&](const auto& dependency) { return invalidGuids.contains(dependency); });
				if (failedDependency == node.Dependencies.end()) continue;
				invalidGuids.insert(guid);
				diagnostics.push_back({ HE::DiagnosticSeverity::Error, "asset.import.dependency_invalid", "Asset dependency could not be included in the import plan", *failedDependency });
				propagatedFailure = true;
			}
		}
		failedAssets = static_cast<uint32_t>(invalidGuids.size());
		for (const auto& guid : invalidGuids) {
			const auto found = nodes.find(guid);
			if (found != nodes.end() && found->second.Record && found->second.Record->Source == HE::AssetSource::Builtin) builtinFailure = true;
			nodes.erase(guid);
		}

		enum class VisitState : uint8_t { Unvisited, Visiting, Visited };
		std::unordered_map<HE::AssetGuid, VisitState> states;
		std::vector<HE::AssetGuid> stack;
		bool cycleFound = false;
		std::function<void(const HE::AssetGuid&)> visit;
		visit = [&](const HE::AssetGuid& guid) {
			if (cycleFound || states[guid] == VisitState::Visited) return;
			if (states[guid] == VisitState::Visiting) return;
			states[guid] = VisitState::Visiting;
			stack.push_back(guid);
			for (const auto& dependency : nodes.at(guid).Dependencies) {
				if (states[dependency] == VisitState::Visiting) {
					const auto cycleBegin = std::find(stack.begin(), stack.end(), dependency);
					std::ostringstream chain;
					for (auto it = cycleBegin; it != stack.end(); ++it) {
						if (it != cycleBegin) chain << " -> ";
						chain << DescribeCycleNode(nodes.at(*it));
					}
					chain << " -> " << DescribeCycleNode(nodes.at(dependency));
					diagnostics.push_back({ HE::DiagnosticSeverity::Error, "asset.import.dependency_cycle", "Asset dependency cycle: " + chain.str(), nodes.at(guid).SourcePath.generic_string() });
					cycleFound = true;
					break;
				}
				visit(dependency);
			}
			stack.pop_back();
			states[guid] = VisitState::Visited;
			if (!cycleFound) output.push_back(nodes.at(guid));
		};

		for (const auto& [guid, node] : nodes) {
			(void)node;
			visit(guid);
			if (cycleFound) break;
		}
		if (cycleFound) {
			failedAssets = static_cast<uint32_t>(std::max<size_t>(1, nodes.size()));
			output.clear();
			auto result = HE::ResultEnvelope::Failure("asset.import.plan", context.GetTargetId(), "Asset dependency cycle detected");
			for (auto& diagnostic : diagnostics) result.AddDetail(std::move(diagnostic));
			return result;
		}
		auto result = HE::ResultEnvelope::Success("asset.import.plan", context.GetTargetId(), "Asset import plan built");
		for (auto& diagnostic : diagnostics) result.AddDetail(std::move(diagnostic));
		return result;
	}
}

namespace HE {
	ResultEnvelope AssetImportService::ImportMissingAssets(
		const ProjectContext& context,
		const AssetManifest& manifest,
		AssetImportReport* outReport) const {
		std::vector<AssetGuid> assetGuids;
		manifest.ForEachRecord([&assetGuids](const AssetManifestRecord& record) {
			if ((record.Source == AssetSource::File || record.Source == AssetSource::Builtin) && record.Kind != AssetKind::Scene) assetGuids.push_back(record.Guid);
		});

		AssetImportReport report;
		auto result = ImportAssets(context, manifest, assetGuids, AssetImportPolicy::MissingOnly, &report);
		result.Operation = "asset.import_missing";
		if (result.Succeeded() && report.FailedAssets == 0) result.Summary = "Missing asset artifacts imported";
		if (outReport) *outReport = std::move(report);
		return result;
	}

	ResultEnvelope AssetImportService::ImportAssets(
		const ProjectContext& context,
		const AssetManifest& manifest,
		std::span<const AssetGuid> assetGuids,
		AssetImportPolicy policy,
		AssetImportReport* outReport) const {
		AssetImportReport report;
		std::vector<ImportPlanNode> plan;
		uint32_t planningFailures = 0;
		bool planningBuiltinFailure = false;
		auto planResult = BuildImportPlan(context, manifest, *m_Registry, *m_Library, assetGuids, policy, plan, planningFailures, planningBuiltinFailure);
		if (!planResult.Succeeded()) {
			report.FailedAssets = planningFailures;
			for (const auto& guid : assetGuids) {
				report.Failures.push_back({ guid, planResult.Details });
			}
			if (outReport) *outReport = report;
			planResult.Operation = "asset.import_assets";
			return planResult;
		}

		auto result = ResultEnvelope::Success("asset.import_assets", context.GetTargetId(), "Asset artifacts imported");
		for (const auto& diagnostic : planResult.Details) result.AddDetail(diagnostic);
		report.FailedAssets = planningFailures;
		bool builtinFailure = planningBuiltinFailure;
		for (const auto& node : plan) {
			const auto& record = *node.Record;
			if (record.Source == AssetSource::Builtin) ++report.TotalBuiltinAssets;
			else ++report.TotalFileAssets;
			if (node.SkipWithoutSource) {
				++report.SkippedAssets;
				continue;
			}

			std::string sourceContentHash;
			auto hashResult = ComputeAssetSourceHash(node.SourcePath, sourceContentHash);
			if (!hashResult.Succeeded()) {
				++report.FailedAssets;
				builtinFailure |= record.Source == AssetSource::Builtin;
				report.Failures.push_back({ record.Guid, hashResult.Details });
				for (auto& diagnostic : hashResult.Details) result.AddDetail(std::move(diagnostic));
				continue;
			}
			const AssetImportContext importContext{
				.Project = context,
				.SourceAsset = record,
				.SourcePath = node.SourcePath,
				.Manifest = &manifest,
				.Library = m_Library,
				.Settings = node.Settings.get()
			};
			AssetImportFingerprintInput fingerprintInput;
			auto fingerprintInputsResult = node.Importer->BuildFingerprintInput(importContext, sourceContentHash, fingerprintInput);
			if (fingerprintInputsResult.Succeeded() && node.Settings) {
				AssetMetaSettingsNode encodedSettings;
				fingerprintInputsResult = node.Importer->EncodeSettings(*node.Settings, encodedSettings);
				std::string settingsDigest;
				if (fingerprintInputsResult.Succeeded()) fingerprintInputsResult = ComputeAssetMetaSettingsDigest(node.Importer->GetSettingsVersion(), encodedSettings, settingsDigest);
				if (fingerprintInputsResult.Succeeded()) fingerprintInput.Options.emplace_back("settings_digest", std::move(settingsDigest));
			}
			std::string importFingerprint;
			auto fingerprintResult = fingerprintInputsResult.Succeeded()
				? ComputeAssetImportFingerprint(fingerprintInput, importFingerprint)
				: fingerprintInputsResult;
			if (!fingerprintInputsResult.Succeeded() || !fingerprintResult.Succeeded()) {
				++report.FailedAssets;
				builtinFailure |= record.Source == AssetSource::Builtin;
				AssetImportFailure failure{ .Guid = record.Guid };
				failure.Diagnostics.insert(failure.Diagnostics.end(), fingerprintInputsResult.Details.begin(), fingerprintInputsResult.Details.end());
				if (fingerprintInputsResult.Succeeded()) {
					failure.Diagnostics.insert(failure.Diagnostics.end(), fingerprintResult.Details.begin(), fingerprintResult.Details.end());
				}
				report.Failures.push_back(std::move(failure));
				for (auto& diagnostic : fingerprintInputsResult.Details) result.AddDetail(std::move(diagnostic));
				if (fingerprintInputsResult.Succeeded()) for (auto& diagnostic : fingerprintResult.Details) result.AddDetail(std::move(diagnostic));
				continue;
			}

			if (!node.Force && m_Library->IsArtifactCurrent(
				record.Guid,
				record.Kind,
				node.Importer->GetId(),
				node.Importer->GetVersion(),
				node.Importer->GetArtifactVersion(),
				importFingerprint)) {
				++report.SkippedAssets;
				continue;
			}

			auto importResult = node.Importer->Import(importContext);
			if (!importResult.Success) {
				++report.FailedAssets;
				builtinFailure |= record.Source == AssetSource::Builtin;
				report.Failures.push_back({ record.Guid, importResult.Diagnostics });
				for (auto& diagnostic : importResult.Diagnostics) result.AddDetail(std::move(diagnostic));
				continue;
			}

			auto commitResult = m_Library->CommitArtifact(
				record.Guid,
				node.Importer->GetId(),
				node.Importer->GetVersion(),
				importFingerprint,
				importResult.Artifact);
			if (!commitResult.Succeeded()) {
				++report.FailedAssets;
				builtinFailure |= record.Source == AssetSource::Builtin;
				report.Failures.push_back({ record.Guid, commitResult.Details });
				for (auto& diagnostic : commitResult.Details) result.AddDetail(std::move(diagnostic));
				continue;
			}

			++report.ImportedAssets;
			report.ImportedAssetGuids.push_back(record.Guid);
		}

		result.SetPayloadValue("total_file_assets", std::to_string(report.TotalFileAssets));
		result.SetPayloadValue("total_builtin_assets", std::to_string(report.TotalBuiltinAssets));
		result.SetPayloadValue("imported_assets", std::to_string(report.ImportedAssets));
		result.SetPayloadValue("skipped_assets", std::to_string(report.SkippedAssets));
		result.SetPayloadValue("failed_assets", std::to_string(report.FailedAssets));
		if (report.FailedAssets > 0) result.Summary = "Asset import completed with per-asset failures";
		if (builtinFailure) {
			result.Status = OperationStatus::Failure;
			result.Summary = "Builtin asset import failed";
		}
		if (outReport) *outReport = report;
		return result;
	}
}
