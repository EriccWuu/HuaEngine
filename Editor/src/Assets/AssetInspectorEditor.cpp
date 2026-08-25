#include "enginepch.h"
#include "Assets/AssetInspectorEditor.h"

#include "HuaEngine/Application.h"
#include "HuaEngine/Application/ApplicationOperations.h"
#include "Selection.h"
#include "Workbench/EditorWorkbenchState.h"
#include "imgui.h"

namespace HE::Editor {
	namespace {
		const char* GetImportHealthLabel(AssetImportHealthState state) {
			switch (state) {
			case AssetImportHealthState::Current: return "Current";
			case AssetImportHealthState::LastGoodWithFailure: return "Last good with failure";
			case AssetImportHealthState::Missing: return "Missing";
			case AssetImportHealthState::Stale: return "Stale";
			case AssetImportHealthState::NotApplicable: return "Not applicable";
			}
			return "Unknown";
		}
	}

	AssetInspectorEditor::AssetInspectorEditor(AssetPickerCatalog& pickerCatalog)
		: m_PickerCatalog(pickerCatalog),
		  m_Host(SceneAssetEditorServices{
			  .OpenScene = [this](const std::filesystem::path& scenePath) {
				  if (m_OpenSceneCallback) m_OpenSceneCallback(scenePath);
			  },
			  .GetActiveDocument = [this] {
				  SceneAssetDocumentState state;
				  if (m_WorkbenchState) {
					  if (const auto* scene = m_WorkbenchState->GetSceneDocumentSummary()) {
						  state.ActiveScenePath = scene->ScenePath;
						  state.Dirty = scene->Dirty;
					  }
				  }
				  return state;
			  }
		  }) {
		Selection::GetService().SetChangeGuard([this](const EditorSelection& selection) {
			if (!HasDirtyEdit()) return true;
			if (const auto* asset = std::get_if<AssetSelection>(&selection);
				asset && asset->Guid == m_Host.GetSession().GetGuid()) return true;
			RequestDirtyResolution([selection]() mutable {
				Selection::GetService().AcceptGuardedSelection(std::move(selection));
			});
			return false;
		});
	}

	AssetInspectorEditor::~AssetInspectorEditor() {
		Selection::GetService().SetChangeGuard({});
	}

	bool AssetInspectorEditor::HasDirtyEdit() const {
		const auto* editor = m_Host.GetEditor();
		return editor != nullptr && editor->IsDirty();
	}

	void AssetInspectorEditor::QueueReload(const AssetGuid& guid) {
		m_PendingReloadGuid = guid;
	}

	void AssetInspectorEditor::ProcessPendingReload() {
		if (!m_PendingReloadGuid) return;
		const auto guid = std::move(*m_PendingReloadGuid);
		m_PendingReloadGuid.reset();
		if (!Selection::HasAssetSelection() || Selection::GetSelectedAssetGuid() != guid) return;

		auto result = m_Host.Open(guid, [](const AssetGuid& assetGuid, AssetInspectionSnapshot& snapshot) {
			return Application::GetInstance().GetOperations().InspectAsset(assetGuid, snapshot);
		});
		if (!result.Succeeded()) RecordResult(result);
	}

	ResultEnvelope AssetInspectorEditor::Apply(AssetApplyState* outState) {
		auto* editor = m_Host.GetEditor();
		const auto guid = m_Host.GetSession().GetGuid();
		AssetApplyState state = AssetApplyState::ValidationFailed;
		if (outState) *outState = state;
		if (!editor || !m_ProjectContext || guid.empty()) {
			return ResultEnvelope::Failure("asset.editor.apply", guid, "No editable asset is active");
		}

		auto result = editor->Validate();
		if (result.Succeeded()) {
			result = Application::GetInstance().GetOperations().ApplyAssetEdit(*m_ProjectContext, editor->BuildCommit(), state);
			if (IsAssetAuthoringDataSaved(state)) QueueReload(guid);
		}
		if (outState) *outState = state;
		RecordResult(result);
		return result;
	}

	void AssetInspectorEditor::Revert() {
		if (auto* editor = m_Host.GetEditor()) editor->Revert();
	}

	bool AssetInspectorEditor::RequestDirtyResolution(std::function<void()> continuation) {
		if (!HasDirtyEdit()) return false;
		m_DirtyContinuation = std::move(continuation);
		m_OpenDirtyPopup = true;
		return true;
	}

	void AssetInspectorEditor::CheckExternalModification() {
		const auto* snapshot = m_Host.GetSession().GetSnapshot();
		if (!snapshot || snapshot->Asset.Source != AssetSource::File) return;
		RecordResult(m_Host.GetSession().CheckExternalModification());
	}

	void AssetInspectorEditor::BindProject(const ProjectContext* projectContext) {
		if (m_ProjectContext == projectContext) return;
		m_ProjectContext = projectContext;
		m_Host.Close();
		m_PendingReloadGuid.reset();
		m_DirtyContinuation = {};
		m_OpenDirtyPopup = false;
	}

	void AssetInspectorEditor::RecordResult(const ResultEnvelope& result) {
		if (m_WorkbenchState) m_WorkbenchState->RecordEvent(result, "Asset Inspector");
	}

	void AssetInspectorEditor::DrawModals() {
		if (m_OpenDirtyPopup) {
			ImGui::OpenPopup("Unsaved Asset Changes");
			m_OpenDirtyPopup = false;
		}
		if (!ImGui::BeginPopupModal("Unsaved Asset Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

		ImGui::TextWrapped("The current asset has unapplied changes. Apply before continuing?");
		ImGui::Spacing();
		if (ImGui::Button("Apply and Continue")) {
			AssetApplyState state = AssetApplyState::ValidationFailed;
			(void)Apply(&state);
			if (IsAssetAuthoringDataSaved(state)) {
				auto continuation = std::move(m_DirtyContinuation);
				ImGui::CloseCurrentPopup();
				if (continuation) continuation();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard and Continue")) {
			Revert();
			auto continuation = std::move(m_DirtyContinuation);
			ImGui::CloseCurrentPopup();
			if (continuation) continuation();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			m_DirtyContinuation = {};
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	bool AssetInspectorEditor::Draw() {
		ProcessPendingReload();
		if (!Selection::HasAssetSelection()) return false;

		const auto guid = Selection::GetSelectedAssetGuid();
		if (!m_Host.GetSession().IsOpen() || m_Host.GetSession().GetGuid() != guid) {
			auto result = m_Host.Open(guid, [](const AssetGuid& assetGuid, AssetInspectionSnapshot& snapshot) {
				return Application::GetInstance().GetOperations().InspectAsset(assetGuid, snapshot);
			});
			if (!result.Succeeded()) RecordResult(result);
		}

		auto* editor = m_Host.GetEditor();
		if (!editor) {
			ImGui::TextDisabled("Asset inspector unavailable.");
			return false;
		}

		const bool dirty = editor->IsDirty();
		const auto* snapshot = m_Host.GetSession().GetSnapshot();
		if (snapshot) {
			ImGui::TextUnformatted(snapshot->Asset.AssetId.c_str());
			ImGui::Text("Kind: %s", ToString(snapshot->Asset.Kind).data());
			ImGui::TextWrapped("GUID: %s", snapshot->Asset.Guid.c_str());
			ImGui::TextWrapped("Path: %s", snapshot->Asset.RelativePath.generic_string().c_str());
			ImGui::Text("Importer: %s (settings v%u)", snapshot->ImporterId.c_str(), snapshot->SettingsVersion);
			ImGui::Text("Import Health: %s%s", GetImportHealthLabel(snapshot->Health.State), dirty ? "  *" : "");
			ImGui::Text("Dependencies: %zu  Dependents: %zu", snapshot->Dependencies.size(), snapshot->Dependents.size());
			if (!snapshot->ImportFingerprint.empty()) ImGui::TextWrapped("Fingerprint: %s", snapshot->ImportFingerprint.c_str());
			for (const auto& diagnostic : snapshot->Diagnostics) ImGui::TextWrapped("%s: %s", diagnostic.Code.c_str(), diagnostic.Message.c_str());
			ImGui::Separator();
		}

		ImGui::BeginDisabled(!dirty || !m_ProjectContext);
		if (ImGui::Button("Apply")) (void)Apply();
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!dirty);
		if (ImGui::Button("Revert")) Revert();
		ImGui::EndDisabled();
		ImGui::SameLine();
		const bool canReimport = snapshot && snapshot->Asset.Source == AssetSource::File && snapshot->Asset.Kind != AssetKind::Scene && m_ProjectContext;
		ImGui::BeginDisabled(!canReimport);
		if (ImGui::Button("Reimport")) {
			const auto path = snapshot->Asset.AbsolutePath;
			auto reimport = [this, path]() {
				AssetReimportReport report;
				auto result = Application::GetInstance().GetOperations().ReimportAssets(*m_ProjectContext, path, &report);
				RecordResult(result);
			};
			if (!RequestDirtyResolution(reimport)) reimport();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Reload")) {
			auto reload = [this, guid]() { QueueReload(guid); };
			if (!RequestDirtyResolution(reload)) reload();
		}
		ImGui::BeginDisabled(!snapshot || snapshot->Asset.Source != AssetSource::File);
		if (ImGui::Button("Check External Changes")) CheckExternalModification();
		ImGui::EndDisabled();
		if (m_Host.GetSession().IsExternallyModified()) {
			ImGui::TextWrapped("Source or metadata changed externally. Reload to replace the working copy, or cancel this notice.");
			if (ImGui::Button("Cancel External Reload")) m_Host.GetSession().DismissExternalModification();
		}
		ImGui::Separator();

		AssetEditorDrawContext context{
			.ShaderAssets = m_PickerCatalog.Get(AssetKind::Shader),
			.TextureAssets = m_PickerCatalog.Get(AssetKind::Texture2D),
			.GetShaderAuthoringMetadata = [](const AssetGuid& shaderGuid, Rendering::ShaderAuthoringMetadata& metadata) {
				return Application::GetInstance().GetOperations().GetShaderAuthoringMetadata(shaderGuid, metadata);
			},
			.ReimportAsset = [this](const std::filesystem::path& path) {
				AssetReimportReport report;
				auto result = m_ProjectContext
					? Application::GetInstance().GetOperations().ReimportAssets(*m_ProjectContext, path, &report)
					: ResultEnvelope::Failure("asset.reimport", path.generic_string(), "Project context is unavailable");
				RecordResult(result);
				return result;
			}
		};
		editor->Draw(context);
		return false;
	}
}
