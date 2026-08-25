#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine/Asset/Library/AssetArtifactIO.h"
#include "HuaEngine/Asset/Library/AssetBinaryIO.h"
#include "HuaEngine/Asset/Library/AssetLibrary.h"
#include "HuaEngine/Asset/Artifact/MeshArtifact.h"
#include "HuaEngine/Asset/Artifact/ShaderArtifact.h"
#include "HuaEngine/Asset/Import/AssetSourceHash.h"
#include "HuaEngine/Core/Sha256.h"
#include "HuaEngine/Project/ProjectContext.h"
#include "HuaEngine/Rendering/Mesh/MeshCore.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[AssetLibrarySmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	void TestBinaryRoundTrip() {
		HE::AssetBinaryWriter writer;
		writer.WriteU8(0x7f);
		writer.WriteU32(0x78563412u);
		writer.WriteU64(0x1122334455667788ull);
		writer.WriteFloat(3.5f);
		writer.WriteString("asset/library");
		writer.WriteBytes({ 1, 3, 5, 7 });

		HE::AssetBinaryReader reader(writer.GetData());
		uint8_t u8 = 0;
		uint32_t u32 = 0;
		uint64_t u64 = 0;
		float floatValue = 0.0f;
		std::string stringValue;
		std::vector<uint8_t> bytes;

		Require(reader.ReadU8(u8) && u8 == 0x7f, "Expected uint8 round-trip");
		Require(reader.ReadU32(u32) && u32 == 0x78563412u, "Expected uint32 little-endian round-trip");
		Require(reader.ReadU64(u64) && u64 == 0x1122334455667788ull, "Expected uint64 little-endian round-trip");
		Require(reader.ReadFloat(floatValue) && floatValue == 3.5f, "Expected float round-trip");
		Require(reader.ReadString(stringValue) && stringValue == "asset/library", "Expected string round-trip");
		Require(reader.ReadBytes(4, bytes) && bytes == std::vector<uint8_t>({ 1, 3, 5, 7 }), "Expected byte payload round-trip");
		Require(reader.Remaining() == 0, "Expected reader to consume the complete buffer");
		Require(!reader.Failed(), "Expected successful reader state");
	}

	void TestTruncatedReadFailsPermanently() {
		const std::vector<uint8_t> truncated = { 1, 2, 3 };
		HE::AssetBinaryReader reader(truncated);
		uint32_t value = 0;
		uint8_t trailing = 0;

		Require(!reader.ReadU32(value), "Expected truncated uint32 read to fail");
		Require(reader.Failed(), "Expected reader to retain its failed state");
		Require(!reader.ReadU8(trailing), "Expected reads after failure to remain rejected");
	}

	void TestOversizedStringFailsBeforeAllocation() {
		HE::AssetBinaryWriter writer;
		writer.WriteU32(1024);

		HE::AssetBinaryReaderLimits limits;
		limits.MaxStringBytes = 32;
		HE::AssetBinaryReader reader(writer.GetData(), limits);
		std::string value;

		Require(!reader.ReadString(value), "Expected oversized string length to be rejected");
		Require(reader.Failed(), "Expected oversized string to fail the reader");
	}

	HE::ProjectContext MakeProjectContext(const std::filesystem::path& root) {
		HE::ProjectContext context;
		context.RootPath = root;
		context.ProjectFilePath = root / ".huaengine" / "project.json";
		return context;
	}

	void WriteFileBytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
		std::filesystem::create_directories(path.parent_path());
		std::ofstream stream(path, std::ios::out | std::ios::binary | std::ios::trunc);
		Require(stream.good(), "Expected binary file write to open");
		stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		Require(stream.good(), "Expected binary file write to succeed");
	}

	void TestSourceContentHash(const std::filesystem::path& root) {
		const auto sourcePath = root / "hash-source.bin";
		WriteFileBytes(sourcePath, { 'a', 'b', 'c' });

		std::string sourceHash;
		Require(HE::ComputeAssetSourceHash(sourcePath, sourceHash).Succeeded(), "Expected source content hash computation");
		Require(
			sourceHash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
			"Expected standard SHA-256 digest");

		WriteFileBytes(sourcePath, std::vector<uint8_t>(1'000'000, 'a'));
		Require(HE::ComputeAssetSourceHash(sourcePath, sourceHash).Succeeded(), "Expected chunked source content hash computation");
		Require(
			sourceHash == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
			"Expected standard chunked SHA-256 digest");
	}

	void TestArtifactCommitAndCatalogRoundTrip(const std::filesystem::path& root) {
		constexpr std::string_view SourceHash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
		const auto context = MakeProjectContext(root);
		HE::AssetLibrary library;
		Require(library.Open(context).Succeeded(), "Expected empty asset library to open");
		Require(std::filesystem::is_directory(root / "Library" / "Artifacts"), "Expected artifact directory creation");

		HE::AssetArtifact artifact;
		Require(HE::EncodeMeshArtifact(*HE::Rendering::Mesh::CreateQuad("LibraryQuad"), artifact).Succeeded(), "Expected valid mesh artifact fixture");
		artifact.Dependencies = { "texture-guid" };

		Require(library.CommitArtifact("mesh-guid", "hua.mesh-yaml", 2, SourceHash, artifact).Succeeded(), "Expected artifact commit to succeed");
		Require(library.Save().Succeeded(), "Expected asset library catalog save to succeed");

		const auto* record = library.Find("mesh-guid");
		Require(record != nullptr, "Expected committed library record");
		Require(record->Kind == HE::AssetKind::Mesh, "Expected committed mesh kind");
		Require(record->ImporterId == "hua.mesh-yaml", "Expected importer id persistence");
		Require(record->ImportFingerprint == SourceHash, "Expected import fingerprint persistence");
		const auto payloadHash = HE::Sha256ToHex(HE::ComputeSha256(artifact.Payload));
		Require(record->ArtifactRelativePath.filename().string().find(payloadHash) != std::string::npos, "Expected payload-addressed artifact candidate path");
		Require(record->Dependencies == artifact.Dependencies, "Expected dependency persistence");
		Require(library.FindDependents("texture-guid") == std::vector<HE::AssetGuid>{ "mesh-guid" }, "Expected reverse dependency lookup");
		Require(library.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, HE::MeshArtifactVersion), "Expected compatible artifact availability");
		Require(library.IsArtifactCurrent("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, HE::MeshArtifactVersion, SourceHash), "Expected matching source hash to be current");
		Require(!library.IsArtifactCurrent("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, HE::MeshArtifactVersion, "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"), "Expected changed source hash to be stale");
		Require(!library.IsArtifactAvailable("mesh-guid", HE::AssetKind::Material, "hua.mesh-yaml", 2, HE::MeshArtifactVersion), "Expected kind mismatch rejection");
		Require(!library.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 9, HE::MeshArtifactVersion), "Expected importer version mismatch rejection");
		Require(!library.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, 9), "Expected artifact version mismatch rejection");

		HE::AssetArtifact loadedArtifact;
		Require(library.ReadArtifact("mesh-guid", loadedArtifact).Succeeded(), "Expected committed artifact read");
		Require(loadedArtifact.Kind == artifact.Kind, "Expected artifact kind round-trip");
		Require(loadedArtifact.ArtifactVersion == artifact.ArtifactVersion, "Expected artifact version round-trip");
		Require(loadedArtifact.Payload == artifact.Payload, "Expected artifact payload round-trip");

		HE::AssetLibrary reopened;
		Require(reopened.Open(context).Succeeded(), "Expected saved asset library to reopen");
		Require(reopened.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, HE::MeshArtifactVersion), "Expected reopened artifact availability");
		Require(reopened.IsArtifactCurrent("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, HE::MeshArtifactVersion, SourceHash), "Expected reopened source hash to remain current");

		const auto reopenedRecord = reopened.Find("mesh-guid");
		Require(reopenedRecord != nullptr, "Expected reopened record");
		const auto artifactPath = root / "Library" / reopenedRecord->ArtifactRelativePath;
		std::filesystem::remove(artifactPath);
		Require(!reopened.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, HE::MeshArtifactVersion), "Expected missing artifact rejection");

		Require(reopened.CommitArtifact("mesh-guid", "hua.mesh-yaml", 2, SourceHash, artifact).Succeeded(), "Expected missing artifact recreation");
		std::fstream corruptStream(artifactPath, std::ios::in | std::ios::out | std::ios::binary);
		Require(corruptStream.good(), "Expected artifact corruption stream");
		const char badMagic = 'X';
		corruptStream.write(&badMagic, 1);
		corruptStream.close();
		Require(!reopened.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, HE::MeshArtifactVersion), "Expected corrupt artifact rejection");
	}

	void TestTransactionalCommitRollback(const std::filesystem::path& root) {
		constexpr std::string_view Fingerprint = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
		const auto context = MakeProjectContext(root);
		HE::AssetLibrary library;
		Require(library.Open(context).Succeeded(), "Expected transaction library open");
		HE::AssetArtifact firstArtifact;
		HE::AssetArtifact secondArtifact;
		Require(HE::EncodeMeshArtifact(*HE::Rendering::Mesh::CreateQuad("First"), firstArtifact).Succeeded(), "Expected first transaction artifact");
		Require(HE::EncodeMeshArtifact(*HE::Rendering::Mesh::CreateQuad("Second"), secondArtifact).Succeeded(), "Expected second transaction artifact");
		Require(library.CommitArtifact("transaction-guid", "hua.mesh-yaml", 2, Fingerprint, firstArtifact).Succeeded(), "Expected first transaction commit");
		const auto firstRelativePath = library.Find("transaction-guid")->ArtifactRelativePath;
		const auto secondRelativePath = std::filesystem::path("Artifacts") / (std::string("transaction-guid-") + HE::Sha256ToHex(HE::ComputeSha256(secondArtifact.Payload)) + ".huamesh");
		Require(firstRelativePath != secondRelativePath, "Expected same-fingerprint payload change to select a new candidate path");

		const auto blockedTemporaryPath = library.GetCatalogPath().string() + ".tmp";
		std::filesystem::create_directories(blockedTemporaryPath);
		WriteFileBytes(std::filesystem::path(blockedTemporaryPath) / "blocked", { 1 });
		Require(library.CommitArtifact("transaction-guid", "hua.mesh-yaml", 2, Fingerprint, secondArtifact).Failed(), "Expected catalog publication failure");
		Require(library.Find("transaction-guid")->ArtifactRelativePath == firstRelativePath, "Expected catalog failure to preserve the in-memory last-good record");
		Require(!std::filesystem::exists(library.GetRootPath() / secondRelativePath), "Expected failed transaction to clean its new artifact candidate");

		HE::AssetLibrary reopened;
		Require(reopened.Open(context).Succeeded(), "Expected last-good catalog reopen after failed transaction");
		Require(reopened.Find("transaction-guid") && reopened.Find("transaction-guid")->ArtifactRelativePath == firstRelativePath, "Expected disk catalog to preserve the last-good record");
		std::filesystem::remove_all(blockedTemporaryPath);
		Require(reopened.CommitArtifact("transaction-guid", "hua.mesh-yaml", 2, Fingerprint, secondArtifact).Succeeded(), "Expected same-fingerprint changed payload commit after catalog recovery");
		Require(reopened.Find("transaction-guid")->ArtifactRelativePath == secondRelativePath, "Expected recovered commit to publish the new payload-addressed record");
		Require(std::filesystem::is_regular_file(reopened.GetRootPath() / firstRelativePath), "Expected content addressing to preserve the previous last-good artifact file");

		HE::AssetArtifact invalidShader;
		invalidShader.Kind = HE::AssetKind::Shader;
		invalidShader.ArtifactVersion = HE::ShaderArtifactVersion;
		invalidShader.Payload = { 1, 2, 3 };
		Require(reopened.CommitArtifact("invalid-shader-guid", "hua.shader-hlsl", 1, Fingerprint, invalidShader).Failed(), "Expected semantic Shader Artifact V2 validation before publication");
		Require(reopened.Find("invalid-shader-guid") == nullptr, "Expected invalid shader candidate not to publish a library record");
	}

	std::vector<uint8_t> MakeOutdatedCatalog() {
		HE::AssetBinaryWriter writer;
		writer.WriteBytes({ 'H', 'U', 'A', 'L', 'I', 'B', 'R', 'Y' });
		writer.WriteU32(1);
		writer.WriteU32(1);
		writer.WriteString("outdated-guid");
		writer.WriteU32(static_cast<uint32_t>(HE::AssetKind::Mesh));
		writer.WriteString("hua.mesh-yaml");
		writer.WriteU32(2);
		writer.WriteU32(3);
		writer.WriteString("Artifacts/outdated-guid.huamesh");
		writer.WriteU32(0);
		return writer.TakeData();
	}

	void TestOutdatedCatalogRebuildsGeneratedCache(const std::filesystem::path& root) {
		const auto context = MakeProjectContext(root);
		WriteFileBytes(root / "Library" / "AssetLibrary.bin", MakeOutdatedCatalog());

		HE::AssetArtifact artifact;
		artifact.Kind = HE::AssetKind::Mesh;
		artifact.ArtifactVersion = 3;
		artifact.Payload = { 1, 2, 3 };
		Require(
			HE::WriteAssetArtifactFile(root / "Library" / "Artifacts" / "outdated-guid.huamesh", artifact).Succeeded(),
			"Expected outdated artifact fixture");

		HE::AssetLibrary library;
		const auto openResult = library.Open(context);
		Require(openResult.Succeeded(), "Expected outdated generated catalog to rebuild automatically");
		Require(!openResult.Details.empty() && openResult.Details.front().Code == "asset.library.catalog_rebuilt", "Expected outdated catalog rebuild diagnostic");
		Require(library.Find("outdated-guid") == nullptr, "Expected outdated catalog records to be discarded");
		Require(!std::filesystem::exists(root / "Library" / "Artifacts" / "outdated-guid.huamesh"), "Expected outdated generated artifacts to be removed");

		std::vector<uint8_t> rebuiltCatalog;
		Require(HE::ReadAssetBinaryFile(root / "Library" / "AssetLibrary.bin", rebuiltCatalog, "smoke.catalog_read").Succeeded(), "Expected rebuilt catalog read");
		HE::AssetBinaryReader reader(rebuiltCatalog);
		std::vector<uint8_t> magic;
		uint32_t version = 0;
		uint32_t recordCount = 1;
		Require(reader.ReadBytes(8, magic) && reader.ReadU32(version) && reader.ReadU32(recordCount), "Expected rebuilt catalog header");
		Require(version == HE::AssetLibraryFormatVersion && recordCount == 0, "Expected empty current-version catalog after rebuild");

		HE::AssetLibrary reopened;
		Require(reopened.Open(context).Succeeded(), "Expected rebuilt catalog reopen");
		Require(reopened.Find("outdated-guid") == nullptr, "Expected rebuilt catalog to remain empty");
	}

	std::vector<uint8_t> MakeEscapingCatalog() {
		HE::AssetBinaryWriter writer;
		writer.WriteBytes({ 'H', 'U', 'A', 'L', 'I', 'B', 'R', 'Y' });
		writer.WriteU32(HE::AssetLibraryFormatVersion);
		writer.WriteU32(1);
		writer.WriteString("escaping-guid");
		writer.WriteU32(static_cast<uint32_t>(HE::AssetKind::Mesh));
		writer.WriteString("hua.mesh-yaml");
		writer.WriteU32(1);
		writer.WriteU32(1);
		writer.WriteString("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
		writer.WriteString("../outside.huamesh");
		writer.WriteU32(0);
		writer.WriteString("");
		writer.WriteU32(0);
		return writer.TakeData();
	}

	void TestInvalidCatalogRebuildsEmpty(const std::filesystem::path& root) {
		const auto context = MakeProjectContext(root);
		WriteFileBytes(root / "Library" / "AssetLibrary.bin", MakeEscapingCatalog());

		HE::AssetLibrary library;
		const auto openResult = library.Open(context);
		Require(openResult.Succeeded(), "Expected generated invalid catalog to rebuild automatically");
		Require(!openResult.Details.empty(), "Expected catalog rebuild diagnostic");
		Require(openResult.Details.front().Code == "asset.library.catalog_rebuilt", "Expected stable rebuild diagnostic code");
		Require(library.Find("escaping-guid") == nullptr, "Expected invalid catalog records to be discarded");
	}
}

int main() {
	TestBinaryRoundTrip();
	TestTruncatedReadFailsPermanently();
	TestOversizedStringFailsBeforeAllocation();

	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineAssetLibrarySmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected smoke root cleanup before test");

	TestSourceContentHash(smokeRoot / "Hash");
	TestArtifactCommitAndCatalogRoundTrip(smokeRoot / "ValidProject");
	TestTransactionalCommitRollback(smokeRoot / "TransactionProject");
	TestOutdatedCatalogRebuildsGeneratedCache(smokeRoot / "LegacyProject");
	TestInvalidCatalogRebuildsEmpty(smokeRoot / "InvalidProject");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected smoke root cleanup after test");

	std::cout << "AssetLibrarySmoke passed" << std::endl;
	return 0;
}
