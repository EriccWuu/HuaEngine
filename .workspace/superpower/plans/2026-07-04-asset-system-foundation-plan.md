# Asset System Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a stable project asset database foundation with `AssetGuid`, `.hua/assets.json`, runtime-only handles, typed component asset references, lazy runtime resolve, scene migration, and fallback rendering.

**Architecture:** Split persistent asset metadata from runtime payloads. Persist stable `AssetGuid` references in scenes/components, keep `AssetHandle` session-only, load runtime resources through `AssetResolver` backed by `AssetRuntimeCache`, and seed builtin/fallback assets through the project manifest.

**Tech Stack:** C++17, CMake, EnTT ECS, existing JSON serialization backend, OpenGL rendering types, Python reflection generator, PowerShell verification commands.

---

## Shared Context

Read these files before starting Task 1:

- `.workspace/superpower/specs/2026-07-04-asset-system-foundation-design.md`
- `CLAUDE.md`
- `HuaEngine/src/HuaEngine/Asset/AssetRegistry.h`
- `HuaEngine/src/HuaEngine/Asset/AssetService.h`
- `HuaEngine/src/HuaEngine/Asset/AssetService.cpp`
- `HuaEngine/src/Module/Rendering/RenderingComponent.h`
- `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h`
- `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.cpp`
- `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`
- `HuaEngine/src/HuaEngine/Serialization/SerializationCore.h`

Do not revert or overwrite unrelated working tree changes. At plan creation time the repository already had unrelated changes in:

- `Tests/TestProj/Scenes/editor_workbench.scene`
- `.workspace/asset-system-refactor-handoff.md`
- `.workspace/cli-p0-handoff.md`

Use `apply_patch` for manual edits. Run commands from repository root:

```powershell
D:\Workspace\VS Workspace\HuaEngine
```

After changing reflected component fields, regenerate reflection output:

```powershell
python Tools/Reflection/reflection_tool.py validate --root .
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
```

## File Structure

Create focused asset files:

- `HuaEngine/src/HuaEngine/Asset/AssetTypes.h`: shared `AssetGuid`, `AssetKind`, builtin enum, source enum, import state enum, typed asset reference structs, string conversion helpers.
- `HuaEngine/src/HuaEngine/Asset/AssetManifest.h`: manifest record/container declarations and manifest load/save API.
- `HuaEngine/src/HuaEngine/Asset/AssetManifest.cpp`: JSON manifest parsing/writing, builtin seeding, path validation helpers.
- `HuaEngine/src/HuaEngine/Asset/AssetRuntimeCache.h`: runtime payload cache declarations.
- `HuaEngine/src/HuaEngine/Asset/AssetResolver.h`: resolver API for mesh/material/texture/material instance.
- `HuaEngine/src/HuaEngine/Asset/AssetResolver.cpp`: lazy load, builtin creation, fallback diagnostics.

Modify existing files:

- `HuaEngine/src/HuaEngine/Asset/AssetRegistry.h`: metadata-only registry.
- `HuaEngine/src/HuaEngine/Asset/AssetService.h`
- `HuaEngine/src/HuaEngine/Asset/AssetService.cpp`
- `HuaEngine/src/HuaEngine/Application/ApplicationServices.h`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- `CLI/src/CLIAssetCommands.h`
- `CLI/src/CLIAssetCommands.cpp`
- `CLI/src/CLICommandCatalog.cpp`
- `HuaEngine/src/Module/Rendering/RenderingComponent.h`
- `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`
- `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h`
- `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.cpp`
- `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h`
- `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`
- `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
- `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h`
- `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
- `HuaEngine/CMakeLists.txt`
- `CLI/CMakeLists.txt`
- `Tests/*.cpp` smoke tests listed per task.

---

### Task 1: Asset Types, Manifest, and Metadata Registry

**Files:**
- Create: `HuaEngine/src/HuaEngine/Asset/AssetTypes.h`
- Create: `HuaEngine/src/HuaEngine/Asset/AssetManifest.h`
- Create: `HuaEngine/src/HuaEngine/Asset/AssetManifest.cpp`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetRegistry.h`
- Modify: `HuaEngine/CMakeLists.txt`
- Test: `Tests/AssetServiceSmoke.cpp`

- [ ] **Step 1: Add failing manifest tests**

Add a first block to `Tests/AssetServiceSmoke.cpp` before existing load/register assertions:

```cpp
const auto manifestPath = projectContext.GetProjectRootPath() / ".hua" / "assets.json";
HE::AssetManifest manifest;
auto initManifestResult = HE::LoadOrCreateAssetManifest(projectContext, manifest);
Require(initManifestResult.Succeeded(), "Expected asset manifest to initialize");
Require(std::filesystem::is_regular_file(manifestPath), "Expected .hua/assets.json to be created");
Require(manifest.FindByGuid(HE::BuiltinAssetGuids::QuadMesh) != nullptr, "Expected builtin quad mesh GUID");
Require(manifest.FindByGuid(HE::BuiltinAssetGuids::FallbackMaterial) != nullptr, "Expected builtin fallback material GUID");
Require(manifest.FindByAssetId("builtin/mesh/quad") != nullptr, "Expected builtin quad asset id lookup");
```

Also add metadata-only registry assertions after an imported mesh record is available:

```cpp
HE::AssetRecord quadRecord;
Require(assetService.ResolveAsset("Meshes/SmokeQuad.mesh", quadRecord).Succeeded(), "Expected asset record lookup by id");
Require(!quadRecord.Guid.empty(), "Expected asset record to have stable guid");
Require(quadRecord.Handle != 0, "Expected runtime handle to remain available");
Require(quadRecord.Kind == HE::AssetKind::Mesh, "Expected mesh asset kind");
```

- [ ] **Step 2: Run test target and verify failure**

Run:

```powershell
cmake --build build --config Debug --target AssetServiceSmoke
```

Expected: compile fails because `AssetManifest`, `LoadOrCreateAssetManifest`, and `BuiltinAssetGuids` do not exist.

- [ ] **Step 3: Implement `AssetTypes.h`**

Create `HuaEngine/src/HuaEngine/Asset/AssetTypes.h`:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace HE {
	using AssetGuid = std::string;
	using AssetHandle = uint64_t;

	enum class AssetKind {
		Unknown,
		Mesh,
		Material,
		Texture2D
	};

	enum class AssetSource {
		Unknown,
		File,
		Builtin
	};

	enum class AssetImportState {
		Unknown,
		Imported,
		Registered,
		Builtin,
		Missing
	};

	enum class BuiltinMeshPrimitive {
		Quad,
		Cube,
		Sphere
	};

	struct AssetReference {
		AssetGuid Guid;

		[[nodiscard]] bool IsValid() const { return !Guid.empty(); }
	};

	struct MeshAssetRef {
		AssetReference Reference;
	};

	struct MaterialAssetRef {
		AssetReference Reference;
	};

	struct TextureAssetRef {
		AssetReference Reference;
	};

	namespace BuiltinAssetGuids {
		inline const AssetGuid QuadMesh = "builtin-mesh-quad";
		inline const AssetGuid CubeMesh = "builtin-mesh-cube";
		inline const AssetGuid SphereMesh = "builtin-mesh-sphere";
		inline const AssetGuid DefaultMaterial = "builtin-material-default";
		inline const AssetGuid FallbackMesh = "builtin-mesh-fallback";
		inline const AssetGuid FallbackMaterial = "builtin-material-fallback";
	}

	std::string GenerateAssetGuid();
	std::string_view ToString(AssetKind kind);
	std::string_view ToString(AssetSource source);
	std::string_view ToString(AssetImportState state);
	std::string_view ToString(BuiltinMeshPrimitive primitive);
	AssetKind AssetKindFromString(std::string_view value);
	AssetSource AssetSourceFromString(std::string_view value);
	AssetImportState AssetImportStateFromString(std::string_view value);
}
```

Place function definitions in `AssetManifest.cpp` to avoid another source file.

- [ ] **Step 4: Implement manifest declarations**

Create `HuaEngine/src/HuaEngine/Asset/AssetManifest.h`:

```cpp
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "AssetTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	struct AssetManifestRecord {
		AssetGuid Guid;
		std::string AssetId;
		AssetKind Kind = AssetKind::Unknown;
		AssetSource Source = AssetSource::Unknown;
		std::filesystem::path RelativePath;
		std::string BuiltinName;
		AssetImportState ImportState = AssetImportState::Unknown;
	};

	class AssetManifest {
	public:
		[[nodiscard]] bool Empty() const { return m_Records.empty(); }
		[[nodiscard]] size_t Size() const { return m_Records.size(); }
		[[nodiscard]] const AssetManifestRecord* FindByGuid(const AssetGuid& guid) const;
		[[nodiscard]] AssetManifestRecord* FindMutableByGuid(const AssetGuid& guid);
		[[nodiscard]] const AssetManifestRecord* FindByAssetId(std::string_view assetId) const;
		[[nodiscard]] bool Upsert(AssetManifestRecord record);

		template<typename Callback>
		void ForEachRecord(Callback&& callback) const {
			for (const auto& record : m_Records) {
				callback(record);
			}
		}

	private:
		std::vector<AssetManifestRecord> m_Records;
		std::unordered_map<AssetGuid, size_t> m_GuidIndex;
		std::unordered_map<std::string, size_t> m_AssetIdIndex;
	};

	ResultEnvelope LoadOrCreateAssetManifest(const ProjectContext& context, AssetManifest& outManifest);
	ResultEnvelope LoadAssetManifest(const ProjectContext& context, AssetManifest& outManifest);
	ResultEnvelope SaveAssetManifest(const ProjectContext& context, const AssetManifest& manifest);
	void SeedBuiltinAssets(AssetManifest& manifest);
	std::filesystem::path GetAssetManifestPath(const ProjectContext& context);
}
```

- [ ] **Step 5: Implement manifest load/save and builtin seed**

Create `HuaEngine/src/HuaEngine/Asset/AssetManifest.cpp` using the existing JSON dependency already available through `enginepch.h` and serialization backend patterns. Write output JSON with this exact top-level shape:

```json
{
  "version": 1,
  "assets": [
    {
      "guid": "builtin-mesh-quad",
      "asset_id": "builtin/mesh/quad",
      "kind": "mesh",
      "source": "builtin",
      "relative_path": "",
      "builtin_name": "quad",
      "import_state": "builtin"
    }
  ]
}
```

Implement `GenerateAssetGuid()` with a process-local random UUID-style hex string:

```cpp
std::string GenerateAssetGuid() {
	static std::random_device randomDevice;
	static std::mt19937_64 generator(randomDevice());
	std::uniform_int_distribution<uint64_t> distribution;

	const uint64_t high = distribution(generator);
	const uint64_t low = distribution(generator);
	std::ostringstream stream;
	stream << std::hex << std::setfill('0')
		<< std::setw(16) << high
		<< std::setw(16) << low;
	return stream.str();
}
```

Seed builtin records exactly:

```cpp
Seed("builtin-mesh-quad", "builtin/mesh/quad", AssetKind::Mesh, "quad");
Seed("builtin-mesh-cube", "builtin/mesh/cube", AssetKind::Mesh, "cube");
Seed("builtin-mesh-sphere", "builtin/mesh/sphere", AssetKind::Mesh, "sphere");
Seed("builtin-material-default", "builtin/material/default", AssetKind::Material, "default");
Seed("builtin-mesh-fallback", "builtin/mesh/fallback", AssetKind::Mesh, "fallback");
Seed("builtin-material-fallback", "builtin/material/fallback", AssetKind::Material, "fallback");
```

- [ ] **Step 6: Make `AssetRegistry` metadata-only**

Modify `AssetRegistry.h`:

- Include `AssetTypes.h`.
- Remove rendering includes.
- Remove `Ref<Mesh>`, `Ref<Material>`, `Ref<Texture2D>` fields from `AssetRecord`.
- Add fields:

```cpp
AssetGuid Guid;
AssetSource Source = AssetSource::Unknown;
std::string BuiltinName;
AssetImportState ImportState = AssetImportState::Unknown;
```

Update `IsOperational()`:

```cpp
[[nodiscard]] bool IsOperational() const {
	return !Guid.empty() && !AssetId.empty() && Kind != AssetKind::Unknown && Source != AssetSource::Unknown;
}
```

Add GUID indexes:

```cpp
std::unordered_map<AssetGuid, AssetHandle> m_Guids;
```

Add `FindByGuid`, `FindMutableByGuid`, and `ContainsGuid`.

- [ ] **Step 7: Add new files to CMake**

Update `HuaEngine/CMakeLists.txt` so `AssetManifest.cpp` is compiled and new headers are included in the source list.

- [ ] **Step 8: Run task verification**

Run:

```powershell
cmake --build build --config Debug --target AssetServiceSmoke
.\build\bin\Debug-Windows-x64\smoke\AssetServiceSmoke.exe
```

Expected: compile may fail in `AssetService.cpp` because runtime payload fields were removed. Fix only metadata-related compile errors needed for Task 1 by setting `Guid`, `Source`, `ImportState`, and avoiding removed payload fields. Full runtime resolve is Task 2.

- [ ] **Step 9: Commit Task 1**

```powershell
git add HuaEngine/src/HuaEngine/Asset/AssetTypes.h HuaEngine/src/HuaEngine/Asset/AssetManifest.h HuaEngine/src/HuaEngine/Asset/AssetManifest.cpp HuaEngine/src/HuaEngine/Asset/AssetRegistry.h HuaEngine/CMakeLists.txt Tests/AssetServiceSmoke.cpp
git commit -m "feat(asset): add manifest metadata registry"
```

---

### Task 2: Runtime Cache and Asset Resolver

**Files:**
- Create: `HuaEngine/src/HuaEngine/Asset/AssetRuntimeCache.h`
- Create: `HuaEngine/src/HuaEngine/Asset/AssetResolver.h`
- Create: `HuaEngine/src/HuaEngine/Asset/AssetResolver.cpp`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetService.h`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetService.cpp`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationServices.h`
- Modify: `HuaEngine/CMakeLists.txt`
- Test: `Tests/AssetServiceSmoke.cpp`

- [ ] **Step 1: Add failing resolver tests**

In `Tests/AssetServiceSmoke.cpp`, add after manifest initialization:

```cpp
HE::AssetResolver resolver(assetService);
HE::Ref<HE::Rendering::Mesh> builtinQuad;
auto builtinQuadResult = resolver.ResolveMesh(HE::BuiltinAssetGuids::QuadMesh, builtinQuad);
Require(builtinQuadResult.Succeeded(), "Expected builtin quad mesh resolve to succeed");
Require(static_cast<bool>(builtinQuad), "Expected builtin quad runtime mesh");

HE::Ref<HE::Rendering::Material> fallbackMaterial;
auto fallbackMaterialResult = resolver.ResolveMaterial(HE::BuiltinAssetGuids::FallbackMaterial, fallbackMaterial);
Require(fallbackMaterialResult.Succeeded(), "Expected fallback material resolve to succeed");
Require(static_cast<bool>(fallbackMaterial), "Expected fallback material runtime object");
```

Add cache reuse assertions after file mesh load:

```cpp
HE::Ref<HE::Rendering::Mesh> resolvedByGuidA;
HE::Ref<HE::Rendering::Mesh> resolvedByGuidB;
Require(resolver.ResolveMesh(quadRecord.Guid, resolvedByGuidA).Succeeded(), "Expected mesh resolve by guid");
Require(resolver.ResolveMesh(quadRecord.Guid, resolvedByGuidB).Succeeded(), "Expected second mesh resolve by guid");
Require(resolvedByGuidA == resolvedByGuidB, "Expected resolver to reuse runtime cache");
```

- [ ] **Step 2: Run test and verify failure**

```powershell
cmake --build build --config Debug --target AssetServiceSmoke
```

Expected: compile fails because `AssetResolver` and runtime cache do not exist.

- [ ] **Step 3: Implement runtime cache**

Create `AssetRuntimeCache.h`:

```cpp
#pragma once

#include <unordered_map>

#include "AssetTypes.h"
#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Mesh/MeshCore.h"
#include "HuaEngine/Rendering/Texture.h"

namespace HE {
	class AssetRuntimeCache {
	public:
		void StoreMesh(const AssetGuid& guid, Ref<Rendering::Mesh> mesh) { m_Meshes[guid] = std::move(mesh); }
		void StoreMaterial(const AssetGuid& guid, Ref<Rendering::Material> material) { m_Materials[guid] = std::move(material); }
		void StoreTexture(const AssetGuid& guid, Ref<Rendering::Texture2D> texture) { m_Textures[guid] = std::move(texture); }

		[[nodiscard]] Ref<Rendering::Mesh> FindMesh(const AssetGuid& guid) const;
		[[nodiscard]] Ref<Rendering::Material> FindMaterial(const AssetGuid& guid) const;
		[[nodiscard]] Ref<Rendering::Texture2D> FindTexture(const AssetGuid& guid) const;

	private:
		std::unordered_map<AssetGuid, Ref<Rendering::Mesh>> m_Meshes;
		std::unordered_map<AssetGuid, Ref<Rendering::Material>> m_Materials;
		std::unordered_map<AssetGuid, Ref<Rendering::Texture2D>> m_Textures;
	};
}
```

Define `Find*` methods inline in `AssetRuntimeCache.h`:

```cpp
[[nodiscard]] Ref<Rendering::Mesh> FindMesh(const AssetGuid& guid) const {
	const auto it = m_Meshes.find(guid);
	return it != m_Meshes.end() ? it->second : nullptr;
}

[[nodiscard]] Ref<Rendering::Material> FindMaterial(const AssetGuid& guid) const {
	const auto it = m_Materials.find(guid);
	return it != m_Materials.end() ? it->second : nullptr;
}

[[nodiscard]] Ref<Rendering::Texture2D> FindTexture(const AssetGuid& guid) const {
	const auto it = m_Textures.find(guid);
	return it != m_Textures.end() ? it->second : nullptr;
}
```

- [ ] **Step 4: Implement resolver API**

Create `AssetResolver.h`:

```cpp
#pragma once

#include "AssetRuntimeCache.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	class AssetService;

	class AssetResolver {
	public:
		explicit AssetResolver(AssetService& service);

		[[nodiscard]] ResultEnvelope ResolveMesh(const AssetGuid& guid, Ref<Rendering::Mesh>& outMesh);
		[[nodiscard]] ResultEnvelope ResolveMaterial(const AssetGuid& guid, Ref<Rendering::Material>& outMaterial);
		[[nodiscard]] ResultEnvelope ResolveTexture(const AssetGuid& guid, Ref<Rendering::Texture2D>& outTexture);

	private:
		AssetService* m_Service = nullptr;

		[[nodiscard]] Ref<Rendering::Mesh> CreateBuiltinMesh(const AssetManifestRecord& record) const;
		[[nodiscard]] Ref<Rendering::Material> CreateBuiltinMaterial(const AssetManifestRecord& record) const;
	};
}
```

- [ ] **Step 5: Give `AssetService` manifest and cache ownership**

Modify `AssetService.h`:

- Include `AssetManifest.h` and `AssetRuntimeCache.h`.
- Add:

```cpp
[[nodiscard]] ResultEnvelope LoadOrCreateManifest(const ProjectContext& context);
[[nodiscard]] const AssetManifest& GetManifest() const { return m_Manifest; }
[[nodiscard]] AssetManifest& GetManifest() { return m_Manifest; }
[[nodiscard]] AssetRuntimeCache& GetRuntimeCache() { return m_RuntimeCache; }
[[nodiscard]] const AssetRuntimeCache& GetRuntimeCache() const { return m_RuntimeCache; }
[[nodiscard]] const AssetRecord* FindRecordByGuid(const AssetGuid& guid) const;
```

- Add members:

```cpp
AssetManifest m_Manifest;
AssetRuntimeCache m_RuntimeCache;
```

- [ ] **Step 6: Implement resolver lazy load**

Create `AssetResolver.cpp`. For mesh:

```cpp
ResultEnvelope AssetResolver::ResolveMesh(const AssetGuid& guid, Ref<Rendering::Mesh>& outMesh) {
	if (guid.empty()) {
		return ResultEnvelope::Failure("asset.resolve_mesh", {}, "Mesh asset guid is empty");
	}

	if (auto cached = m_Service->GetRuntimeCache().FindMesh(guid)) {
		outMesh = cached;
		return ResultEnvelope::Success("asset.resolve_mesh", guid, "Mesh asset resolved from runtime cache");
	}

	const auto* record = m_Service->GetAssetRegistry().FindByGuid(guid);
	if (!record || record->Kind != AssetKind::Mesh) {
		return ResultEnvelope::Failure("asset.resolve_mesh", guid, "Mesh asset metadata was not found");
	}

	Ref<Rendering::Mesh> mesh = nullptr;
	if (record->Source == AssetSource::Builtin) {
		mesh = CreateBuiltinMesh(*record);
	}
	else if (record->Source == AssetSource::File) {
		mesh = Rendering::Mesh::LoadFromFile(record->AbsolutePath.generic_string());
	}

	if (!mesh) {
		return ResultEnvelope::ManualIntervention("asset.resolve_mesh", guid, "Mesh asset could not be loaded");
	}

	m_Service->GetRuntimeCache().StoreMesh(guid, mesh);
	outMesh = mesh;
	return ResultEnvelope::Success("asset.resolve_mesh", guid, "Mesh asset resolved");
}
```

For builtin mesh, map `quad`, `cube`, `sphere`, and `fallback` to existing mesh factory methods. Use cube for fallback if no error mesh exists.

For builtin material, call existing `MaterialLibrary::CreateDefaultMaterials()` and use default material for `default`. For fallback, create or reuse a material named `builtin/material/fallback`; use the default material implementation first if there is no error-material API.

- [ ] **Step 7: Update registration/load paths to seed metadata and cache**

In `AssetService.cpp`:

- `LoadOrCreateManifest` calls `HE::LoadOrCreateAssetManifest`, seeds `m_Registry` from manifest records, and stores `m_Manifest`.
- Register/load methods create or update manifest records with GUID.
- Runtime payloads are stored in `m_RuntimeCache`.
- `ResolveMeshAsset(AssetHandle, Ref<Mesh>&)` looks up handle to GUID and delegates to `AssetResolver`.
- `ResolveMaterialAsset` and `ResolveTextureAsset` follow the same pattern.

Use this registration shape:

```cpp
AssetManifestRecord manifestRecord;
manifestRecord.Guid = existingRecord ? existingRecord->Guid : GenerateAssetGuid();
manifestRecord.AssetId = normalizedPath.AssetId;
manifestRecord.Kind = AssetKind::Mesh;
manifestRecord.Source = AssetSource::File;
manifestRecord.RelativePath = normalizedPath.RelativePath;
manifestRecord.ImportState = AssetImportState::Registered;
m_Manifest.Upsert(manifestRecord);
SaveAssetManifest(context, m_Manifest);
```

Then create `AssetRecord` from the manifest record and `Upsert` it into the registry.

- [ ] **Step 8: Add files to CMake**

Add `AssetResolver.cpp` and new headers to `HuaEngine/CMakeLists.txt`.

- [ ] **Step 9: Run task verification**

```powershell
cmake --build build --config Debug --target AssetServiceSmoke
.\build\bin\Debug-Windows-x64\smoke\AssetServiceSmoke.exe
```

Expected: asset service smoke passes and verifies builtin resolve plus cache reuse.

- [ ] **Step 10: Commit Task 2**

```powershell
git add HuaEngine/src/HuaEngine/Asset/AssetRuntimeCache.h HuaEngine/src/HuaEngine/Asset/AssetResolver.h HuaEngine/src/HuaEngine/Asset/AssetResolver.cpp HuaEngine/src/HuaEngine/Asset/AssetService.h HuaEngine/src/HuaEngine/Asset/AssetService.cpp HuaEngine/src/HuaEngine/Application/ApplicationServices.h HuaEngine/CMakeLists.txt Tests/AssetServiceSmoke.cpp
git commit -m "feat(asset): add runtime resolver cache"
```

---

### Task 3: Asset Application Operations and CLI Workflow

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `CLI/src/CLIAssetCommands.h`
- Modify: `CLI/src/CLIAssetCommands.cpp`
- Modify: `CLI/src/CLICommandCatalog.cpp`
- Test: `Tests/ApplicationOperationsSmoke.cpp`
- Test: `Tests/CLIWorkflowSmoke.cpp`
- Test: `Tests/CLIHostSmoke.cpp`

- [ ] **Step 1: Add failing application operation tests**

In `Tests/ApplicationOperationsSmoke.cpp`, assert new operations are published:

```cpp
Require(operations.Supports("asset.manifest.init"), "Expected asset.manifest.init to be published");
Require(operations.Supports("asset.import"), "Expected asset.import to be published");
Require(operations.Supports("asset.list"), "Expected asset.list to be published");
```

Add an operation call after project context creation:

```cpp
auto manifestInit = operations.InitializeAssetManifest(projectContext);
Require(manifestInit.Succeeded(), "Expected manifest init operation to succeed");
Require(!manifestInit.GetPayloadValue("manifest_path").empty(), "Expected manifest path payload");
```

- [ ] **Step 2: Add failing CLI workflow tests**

In `Tests/CLIWorkflowSmoke.cpp`, add command expectations:

```cpp
{
	const auto response = RunCLI({ "asset", "manifest", "init", "--project", projectRoot.string() });
	Require(response.Result.Succeeded(), "Expected asset manifest init command to succeed");
	Require(Contains(response.Output, "\"operation\":\"asset.manifest.init\""), "Expected manifest init operation JSON");
}
{
	const auto response = RunCLI({ "asset", "list", "--project", projectRoot.string() });
	Require(response.Result.Succeeded(), "Expected asset list command to succeed");
	Require(Contains(response.Output, "builtin/mesh/quad"), "Expected builtin quad in asset list output");
}
```

- [ ] **Step 3: Run tests and verify failure**

```powershell
cmake --build build --config Debug --target ApplicationOperationsSmoke
cmake --build build --config Debug --target CLIWorkflowSmoke
```

Expected: compile fails because new operations do not exist.

- [ ] **Step 4: Add application operation methods**

In `ApplicationOperations.h`, add:

```cpp
[[nodiscard]] ResultEnvelope InitializeAssetManifest(const ProjectContext& context) const;
[[nodiscard]] ResultEnvelope ImportAsset(const ProjectContext& context, std::string_view assetId, AssetKind kind, AssetGuid* outGuid = nullptr) const;
[[nodiscard]] ResultEnvelope ListAssets(const ProjectContext& context, std::vector<AssetRecord>& outRecords) const;
```

In `ApplicationOperations.cpp`, implement:

```cpp
ResultEnvelope ApplicationOperations::InitializeAssetManifest(const ProjectContext& context) const {
	auto result = m_Services->Assets().LoadOrCreateManifest(context);
	result.Operation = "asset.manifest.init";
	result.SetPayloadValue("manifest_path", GetAssetManifestPath(context).generic_string());
	return result;
}
```

For `ImportAsset`, call `LoadMeshAsset`, `LoadMaterialAsset`, or `RegisterTextureAsset` based on `kind`. Return the resolved GUID in payload `asset_guid`.

For `ListAssets`, ensure manifest is loaded, iterate registry records into `outRecords`, return payload `asset_count`.

Register operations in `RegisterBuiltInOperations()`:

```cpp
m_Registry.Register({ "asset.manifest.init", OperationDomain::Asset, "Initialize the project asset manifest" });
m_Registry.Register({ "asset.import", OperationDomain::Asset, "Import a single project asset into the manifest" });
m_Registry.Register({ "asset.list", OperationDomain::Asset, "List project manifest assets" });
```

- [ ] **Step 5: Add CLI commands**

In `CLICommandCatalog.cpp`, add definitions:

```cpp
Register({ { "asset", "manifest", "init" }, "Initialize project asset manifest" });
Register({ { "asset", "import" }, "Import a single asset file" });
Register({ { "asset", "list" }, "List project assets" });
```

In `CLIAssetCommands.cpp`:

- `asset manifest init --project <path>` resolves project and calls `InitializeAssetManifest`.
- `asset import --project <path> --asset-id <relative-path> --kind mesh|material|texture2d` parses kind and calls `ImportAsset`.
- `asset list --project <path>` resolves project, calls `ListAssets`, and writes records into response payload using existing `CLIJsonWriter` patterns.

Use exact operation names:

```cpp
"asset.manifest.init"
"asset.import"
"asset.list"
```

- [ ] **Step 6: Keep old CLI compatibility**

Ensure existing command `asset register-default-mesh` still works by delegating to `CreateBuiltinMeshAsset` and producing the previous success operation payload. Do not remove old tests.

- [ ] **Step 7: Run task verification**

```powershell
cmake --build build --config Debug --target ApplicationOperationsSmoke
cmake --build build --config Debug --target CLIWorkflowSmoke
cmake --build build --config Debug --target CLIHostSmoke
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\CLIWorkflowSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\CLIHostSmoke.exe
```

Expected: all three smoke tests pass.

- [ ] **Step 8: Commit Task 3**

```powershell
git add HuaEngine/src/HuaEngine/Application/ApplicationOperations.h HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp CLI/src/CLIAssetCommands.h CLI/src/CLIAssetCommands.cpp CLI/src/CLICommandCatalog.cpp Tests/ApplicationOperationsSmoke.cpp Tests/CLIWorkflowSmoke.cpp Tests/CLIHostSmoke.cpp
git commit -m "feat(asset): expose manifest cli operations"
```

---

### Task 4: Component Contracts, Reflection, and Scene Migration

**Files:**
- Modify: `HuaEngine/src/Module/Rendering/RenderingComponent.h`
- Modify: `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `HuaEngine/src/HuaEngine/Serialization/SerializationCore.h`
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h`
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
- Test: `Tests/ECSSceneSerializationSmoke.cpp`
- Test: `Tests/ReflectionGeneratedSmoke.cpp`
- Test: `Tests/SerializationPolicySmoke.cpp`
- Test: `Tests/EditorInspectorRuntimeSmoke.cpp`

- [ ] **Step 1: Add failing reflection tests**

In `Tests/ReflectionGeneratedSmoke.cpp`, replace `MeshAssetName` expectation:

```cpp
Require(HasField(mesh->Fields, "Mesh"), "Expected MeshComponent Mesh field");
Require(!HasField(mesh->Fields, "MeshAssetName"), "Expected MeshAssetName to be removed");
Require(!HasField(mesh->Fields, "m_CachedVertexArray"), "Expected MeshComponent cache field to be omitted");
```

For `MaterialComponent`, expect:

```cpp
Require(HasField(material->Fields, "Material"), "Expected MaterialComponent Material field");
Require(HasField(material->Fields, "Overrides"), "Expected MaterialComponent Overrides field");
Require(!HasField(material->Fields, "MaterialInstance"), "Expected MaterialInstance field to be removed");
```

- [ ] **Step 2: Add failing scene serialization tests**

In `Tests/ECSSceneSerializationSmoke.cpp`, create a scene entity with:

```cpp
HE::Rendering::MeshComponent meshComponent;
meshComponent.Mesh.Reference.Guid = HE::BuiltinAssetGuids::QuadMesh;

HE::Rendering::MaterialComponent materialComponent;
materialComponent.Material.Reference.Guid = HE::BuiltinAssetGuids::DefaultMaterial;
materialComponent.Overrides.SetVec4("u_BaseColor", glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
```

After save, read the scene file text and assert:

```cpp
Require(savedSceneText.find("MeshAssetName") == std::string::npos, "Expected new scene format to omit MeshAssetName");
Require(savedSceneText.find("MaterialInstance") == std::string::npos, "Expected new scene format to omit MaterialInstance");
Require(savedSceneText.find("builtin-mesh-quad") != std::string::npos, "Expected mesh GUID in scene");
Require(savedSceneText.find("builtin-material-default") != std::string::npos, "Expected material GUID in scene");
```

Add a legacy scene JSON fixture containing `MeshAssetName` and `MaterialInstance`; after load, assert migrated component GUIDs are set and saving omits legacy fields.

- [ ] **Step 3: Run tests and verify failure**

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
cmake --build build --config Debug --target ECSSceneSerializationSmoke
```

Expected: compile fails because fields and override type do not exist.

- [ ] **Step 4: Add serializable asset references and material overrides**

In `RenderingComponent.h`, include `HuaEngine/Asset/AssetTypes.h`.

Add:

```cpp
struct MaterialOverrideSet {
	std::unordered_map<std::string, HE::Rendering::MaterialParameterValue> Parameters;

	void SetVec4(const std::string& name, const glm::vec4& value) {
		Parameters[name] = value;
	}

	[[nodiscard]] bool Empty() const {
		return Parameters.empty();
	}
};
```

Use the existing `MaterialParameterValue` variant type from `MaterialTypes.h`. If that type is not directly public, move the override type to a small header beside material types and include it.

Add serializers for `AssetReference`, `MeshAssetRef`, `MaterialAssetRef`, `TextureAssetRef`, and `MaterialOverrideSet` in `SerializationCore.h`.

Serialize references as:

```json
"Mesh": {
  "guid": "builtin-mesh-quad"
}
```

Serialize material overrides as:

```json
"Overrides": {
  "parameters": {
    "u_BaseColor": {
      "type": "vec4",
      "value": [1.0, 0.0, 1.0, 1.0]
    }
  }
}
```

- [ ] **Step 5: Update components**

Change `MeshComponent`:

```cpp
HE_REFLECT_COMPONENT(DisplayName="Mesh", Category="Rendering")
struct MeshComponent : Component {
	MeshComponent() = default;
	explicit MeshComponent(const MeshAssetRef& mesh)
		: Mesh(mesh) {}

	HE_REFLECT_FIELD()
	MeshAssetRef Mesh;
};
```

Change `MaterialComponent`:

```cpp
HE_REFLECT_COMPONENT(DisplayName="Material", Category="Rendering")
struct MaterialComponent : Component {
	MaterialComponent() = default;
	explicit MaterialComponent(const MaterialAssetRef& material)
		: Material(material) {}

	HE_REFLECT_FIELD()
	MaterialAssetRef Material;
	HE_REFLECT_FIELD()
	MaterialOverrideSet Overrides;
	HE_REFLECT_FIELD()
	MaterialBlendMode BlendMode = MaterialBlendMode::Opaque;
};
```

Change `CameraComponent` to remove serialized runtime ownership. Keep runtime `Ref<Camera>` out of reflected fields. If existing code still needs a runtime camera object, use a non-reflected field named `RuntimeCamera` and document it with an English comment:

```cpp
Ref<HE::Rendering::Camera> RuntimeCamera;
```

- [ ] **Step 6: Update default component creation**

In `ApplicationOperations.cpp`:

```cpp
HE::Rendering::MeshComponent MakeDefaultMeshComponent() {
	HE::Rendering::MeshComponent component;
	component.Mesh.Reference.Guid = HE::BuiltinAssetGuids::QuadMesh;
	return component;
}

HE::Rendering::MaterialComponent MakeDefaultMaterialComponent() {
	HE::Rendering::MaterialComponent component;
	component.Material.Reference.Guid = HE::BuiltinAssetGuids::DefaultMaterial;
	return component;
}
```

Do not create `MaterialInstance` in component creation.

- [ ] **Step 7: Implement scene legacy migration**

In `SceneSerializer.cpp`, when reading a component object:

- If `MeshComponent` has `MeshAssetName`, create `Mesh.Guid` by resolving the old name:
  - `"Quad"` -> `BuiltinAssetGuids::QuadMesh`
  - `"Cube"` -> `BuiltinAssetGuids::CubeMesh`
  - `"Sphere"` -> `BuiltinAssetGuids::SphereMesh`
  - AssetId path -> manifest lookup by `AssetId`
- If `MaterialComponent` has `MaterialInstance`, set `Material.Guid` to `BuiltinAssetGuids::DefaultMaterial` when no better base material asset id exists, and migrate parameter overrides into `Overrides`.

Record migration failures through existing scene load diagnostics or log warnings. Do not fail scene load only because a legacy asset cannot be mapped.

- [ ] **Step 8: Regenerate reflection**

```powershell
python Tools/Reflection/reflection_tool.py validate --root .
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
```

- [ ] **Step 9: Run task verification**

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
cmake --build build --config Debug --target ECSSceneSerializationSmoke
cmake --build build --config Debug --target SerializationPolicySmoke
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ECSSceneSerializationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\SerializationPolicySmoke.exe
.\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
```

Expected: tests pass, new reflected fields are present, old serialized fields are absent after save.

- [ ] **Step 10: Commit Task 4**

```powershell
git add HuaEngine/src/Module/Rendering/RenderingComponent.h HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp HuaEngine/src/HuaEngine/Serialization/SerializationCore.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp Tests/ECSSceneSerializationSmoke.cpp Tests/ReflectionGeneratedSmoke.cpp Tests/SerializationPolicySmoke.cpp Tests/EditorInspectorRuntimeSmoke.cpp .workspace/reflection/reflection_manifest.json
git commit -m "refactor(scene): persist typed asset references"
```

---

### Task 5: Render Extraction, Resolver Integration, and Fallback Diagnostics

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.cpp`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.cpp`
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
- Test: `Tests/RenderingOperationsSmoke.cpp`

- [ ] **Step 1: Add failing render fallback tests**

In `Tests/RenderingOperationsSmoke.cpp`, update renderability checks to avoid `mesh.GetVertexArray()` and `material.MaterialInstance`.

Add a scene item with intentionally missing GUIDs:

```cpp
HE::Rendering::MeshComponent missingMesh;
missingMesh.Mesh.Reference.Guid = "missing-mesh-guid";
HE::Rendering::MaterialComponent missingMaterial;
missingMaterial.Material.Reference.Guid = "missing-material-guid";
```

After render:

```cpp
const auto& renderResult = renderSystem->GetLastRenderResult();
Require(renderResult.Succeeded, "Expected render to succeed with fallback resources");
Require(renderResult.Stats.SkippedItems == 0, "Expected fallback resources to avoid skipping render item");
Require(!renderResult.Diagnostics.empty(), "Expected diagnostics for missing asset fallback");
Require(HasDiagnostic(renderResult.Diagnostics, HE::Rendering::RenderDiagnosticCode::FallbackResourceUsed), "Expected fallback diagnostic");
```

Add helper:

```cpp
bool HasDiagnostic(const std::vector<HE::Rendering::RenderDiagnostic>& diagnostics, HE::Rendering::RenderDiagnosticCode code) {
	return std::any_of(diagnostics.begin(), diagnostics.end(), [code](const auto& diagnostic) {
		return diagnostic.Code == code;
	});
}
```

- [ ] **Step 2: Run test and verify failure**

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
```

Expected: compile fails because render types still use old fields and diagnostic enum lacks fallback code.

- [ ] **Step 3: Update render types**

In `RenderTypes.h`, change `RenderItem`:

```cpp
struct MaterialOverrideKey {
	AssetGuid MaterialGuid;
	const MaterialOverrideSet* Overrides = nullptr;
};

struct RenderItem {
	Entity SourceEntity;
	glm::mat4 Transform = glm::mat4(1.0f);
	MeshAssetRef Mesh;
	MaterialAssetRef Material;
	MaterialOverrideSet MaterialOverrides;
};
```

Add diagnostic code:

```cpp
FallbackResourceUsed
```

Add stats field:

```cpp
uint32_t FallbackItems = 0;
```

- [ ] **Step 4: Update extraction**

In `SceneRenderExtractor.cpp`:

```cpp
item.Mesh = mesh.Mesh;
item.Material = material.Material;
item.MaterialOverrides = material.Overrides;
```

No `MeshAssetName`, no `MaterialInstanceRef`.

- [ ] **Step 5: Update resolver dependency**

In `RenderResourceResolver.h`, store an `AssetResolver*`:

```cpp
explicit RenderResourceResolver(HE::AssetResolver& assetResolver);
HE::AssetResolver* m_AssetResolver = nullptr;
```

In `RenderSystem.cpp`, keep render system construction independent of application services and add a setter:

```cpp
void SetAssetResolver(HE::AssetResolver* resolver);
```

and call it from `ApplicationOperations::AttachSceneViewportRenderer`.

- [ ] **Step 6: Implement fallback resolve**

In `RenderResourceResolver.cpp`:

- Resolve mesh via `AssetResolver::ResolveMesh(item.Mesh.Reference.Guid, mesh)`.
- On failure, resolve `BuiltinAssetGuids::FallbackMesh`, increment `FallbackItems`, and add diagnostic.
- Resolve material via `AssetResolver::ResolveMaterial(item.Material.Reference.Guid, material)`.
- On failure, resolve `BuiltinAssetGuids::FallbackMaterial`, increment `FallbackItems`, and add diagnostic.
- Create `MaterialInstance` from base material and apply `item.MaterialOverrides`.

Diagnostic message format:

```cpp
"Asset resolve failed for " + requestedGuid + "; using fallback " + fallbackGuid
```

- [ ] **Step 7: Run task verification**

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Expected: render operation succeeds and reports fallback diagnostics for missing GUIDs.

- [ ] **Step 8: Commit Task 5**

```powershell
git add HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.cpp HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.cpp HuaEngine/src/Module/Rendering/RenderSystem.cpp Tests/RenderingOperationsSmoke.cpp
git commit -m "refactor(rendering): resolve assets through resolver"
```

---

### Task 6: Full Validation Pass and Compatibility Cleanup

**Files:**
- Modify: `Tests/AgentHostAdapterSmoke.cpp`
- Modify: `Tests/HostConsistencySmoke.cpp`
- Modify: `Tests/ValidationServiceSmoke.cpp`
- Modify: `Tests/ProjectWorkbenchSmoke.cpp`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Editor/src/Panels/RuntimeInspector.cpp`
- Modify: `HuaEngine/src/HuaEngine/Validation/ValidationService.cpp`
- Modify: `HuaEngine/src/HuaEngine/Automation/AgentHostAdapter.cpp`

- [ ] **Step 1: Run reflection generation and full target build**

```powershell
python Tools/Reflection/reflection_tool.py validate --root .
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target HuaEngine
cmake --build build --config Debug --target Editor
cmake --build build --config Debug --target HuaEngineCLI
```

Expected: build succeeds. If compile fails because callers still use `MeshAssetName`, `MaterialInstance`, or `GetVertexArray()`, update those callers to the new asset reference or resolver APIs.

- [ ] **Step 2: Update validation service for manifest model**

In `ValidationService.cpp`, ensure asset validation calls `AssetService::ValidateRegistry` after `LoadOrCreateManifest`. Expected validation payload includes:

```text
asset_count
metadata_issue_count
runtime_issue_count
fallback_asset_count
```

Do not treat fallback rendering as asset validity. Missing file or failed runtime load is still a validation issue.

- [ ] **Step 3: Update agent host and host consistency expectations**

Where tests invoke `asset.create_builtin_mesh`, keep expected success. Add payload expectations:

```cpp
Require(!assetCreate.Result.GetPayloadValue("asset_guid").empty(), "Expected asset create payload to include asset guid");
Require(assetCreate.Result.GetPayloadValue("asset_handle") != "0", "Expected runtime handle payload");
```

- [ ] **Step 4: Update editor runtime inspector expectations**

In `RuntimeInspector.cpp`, display typed asset references as GUID string fields for this phase. The first version does not need a full asset picker.

Expected behavior:

- `MeshComponent.Mesh` is visible.
- `MaterialComponent.Material` is visible.
- `MaterialComponent.Overrides` is visible or non-editable with a clear runtime field kind.
- `MaterialInstance` is not visible.

- [ ] **Step 5: Run priority smoke tests**

```powershell
cmake --build build --config Debug --target AssetServiceSmoke
cmake --build build --config Debug --target ApplicationOperationsSmoke
cmake --build build --config Debug --target ValidationServiceSmoke
cmake --build build --config Debug --target ECSSceneSerializationSmoke
cmake --build build --config Debug --target ReflectionGeneratedSmoke
cmake --build build --config Debug --target SerializationPolicySmoke
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
cmake --build build --config Debug --target CLIWorkflowSmoke
cmake --build build --config Debug --target CLIHostSmoke
cmake --build build --config Debug --target RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\AssetServiceSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ValidationServiceSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ECSSceneSerializationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\SerializationPolicySmoke.exe
.\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\CLIWorkflowSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\CLIHostSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Expected: all listed smoke tests pass.

- [ ] **Step 6: Run status and inspect diff**

```powershell
git status --short
git diff --stat
```

Expected: only files related to asset system foundation are modified. Existing unrelated user changes remain unmodified unless explicitly touched by this implementation.

- [ ] **Step 7: Commit Task 6**

```powershell
git add HuaEngine/src Tests CLI Editor .workspace/reflection/reflection_manifest.json
git commit -m "test(asset): validate asset foundation workflow"
```

---

## Final Verification

After all tasks:

```powershell
python Tools/Reflection/reflection_tool.py validate --root .
cmake --build build --config Debug --target HuaEngine
cmake --build build --config Debug --target Editor
cmake --build build --config Debug --target HuaEngineCLI
cmake --build build --config Debug --target AssetServiceSmoke
cmake --build build --config Debug --target ApplicationOperationsSmoke
cmake --build build --config Debug --target ValidationServiceSmoke
cmake --build build --config Debug --target ECSSceneSerializationSmoke
cmake --build build --config Debug --target ReflectionGeneratedSmoke
cmake --build build --config Debug --target SerializationPolicySmoke
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
cmake --build build --config Debug --target CLIWorkflowSmoke
cmake --build build --config Debug --target CLIHostSmoke
cmake --build build --config Debug --target RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\AssetServiceSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ValidationServiceSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ECSSceneSerializationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\SerializationPolicySmoke.exe
.\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\CLIWorkflowSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\CLIHostSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Final acceptance:

- `.hua/assets.json` initializes, saves, loads, and includes stable builtin GUIDs.
- Scene/component persistent resource references use GUIDs, not `AssetHandle` or runtime `Ref<T>`.
- `AssetRegistry` contains metadata only.
- Runtime objects resolve through `AssetResolver` and cache in `AssetRuntimeCache`.
- Old scene fields migrate on read and do not save back out.
- Renderer uses fallback resources with diagnostics for missing or failed assets.
- CLI supports manifest init, import/register, resolve, validate, and list.
