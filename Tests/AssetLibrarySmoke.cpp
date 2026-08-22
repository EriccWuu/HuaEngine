#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine/Asset/Library/AssetArtifactIO.h"
#include "HuaEngine/Asset/Library/AssetBinaryIO.h"
#include "HuaEngine/Asset/Library/AssetLibrary.h"
#include "HuaEngine/Project/ProjectContext.h"

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

	void TestArtifactCommitAndCatalogRoundTrip(const std::filesystem::path& root) {
		const auto context = MakeProjectContext(root);
		HE::AssetLibrary library;
		Require(library.Open(context).Succeeded(), "Expected empty asset library to open");
		Require(std::filesystem::is_directory(root / "Library" / "Artifacts"), "Expected artifact directory creation");

		HE::AssetArtifact artifact;
		artifact.Kind = HE::AssetKind::Mesh;
		artifact.ArtifactVersion = 3;
		artifact.Payload = { 9, 8, 7, 6 };
		artifact.Dependencies = { "texture-guid" };

		Require(library.CommitArtifact("mesh-guid", "hua.mesh-yaml", 2, artifact).Succeeded(), "Expected artifact commit to succeed");
		Require(library.Save().Succeeded(), "Expected asset library catalog save to succeed");

		const auto* record = library.Find("mesh-guid");
		Require(record != nullptr, "Expected committed library record");
		Require(record->Kind == HE::AssetKind::Mesh, "Expected committed mesh kind");
		Require(record->ImporterId == "hua.mesh-yaml", "Expected importer id persistence");
		Require(record->Dependencies == artifact.Dependencies, "Expected dependency persistence");
		Require(library.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, 3), "Expected compatible artifact availability");
		Require(!library.IsArtifactAvailable("mesh-guid", HE::AssetKind::Material, "hua.mesh-yaml", 2, 3), "Expected kind mismatch rejection");
		Require(!library.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 9, 3), "Expected importer version mismatch rejection");
		Require(!library.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, 9), "Expected artifact version mismatch rejection");

		HE::AssetArtifact loadedArtifact;
		Require(library.ReadArtifact("mesh-guid", loadedArtifact).Succeeded(), "Expected committed artifact read");
		Require(loadedArtifact.Kind == artifact.Kind, "Expected artifact kind round-trip");
		Require(loadedArtifact.ArtifactVersion == artifact.ArtifactVersion, "Expected artifact version round-trip");
		Require(loadedArtifact.Payload == artifact.Payload, "Expected artifact payload round-trip");

		HE::AssetLibrary reopened;
		Require(reopened.Open(context).Succeeded(), "Expected saved asset library to reopen");
		Require(reopened.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, 3), "Expected reopened artifact availability");

		const auto reopenedRecord = reopened.Find("mesh-guid");
		Require(reopenedRecord != nullptr, "Expected reopened record");
		const auto artifactPath = root / "Library" / reopenedRecord->ArtifactRelativePath;
		std::filesystem::remove(artifactPath);
		Require(!reopened.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, 3), "Expected missing artifact rejection");

		Require(reopened.CommitArtifact("mesh-guid", "hua.mesh-yaml", 2, artifact).Succeeded(), "Expected missing artifact recreation");
		std::fstream corruptStream(artifactPath, std::ios::in | std::ios::out | std::ios::binary);
		Require(corruptStream.good(), "Expected artifact corruption stream");
		const char badMagic = 'X';
		corruptStream.write(&badMagic, 1);
		corruptStream.close();
		Require(!reopened.IsArtifactAvailable("mesh-guid", HE::AssetKind::Mesh, "hua.mesh-yaml", 2, 3), "Expected corrupt artifact rejection");
	}

	std::vector<uint8_t> MakeEscapingCatalog() {
		HE::AssetBinaryWriter writer;
		writer.WriteBytes({ 'H', 'U', 'A', 'L', 'I', 'B', 'R', 'Y' });
		writer.WriteU32(1);
		writer.WriteU32(1);
		writer.WriteString("escaping-guid");
		writer.WriteU32(static_cast<uint32_t>(HE::AssetKind::Mesh));
		writer.WriteString("hua.mesh-yaml");
		writer.WriteU32(1);
		writer.WriteU32(1);
		writer.WriteString("../outside.huamesh");
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

	TestArtifactCommitAndCatalogRoundTrip(smokeRoot / "ValidProject");
	TestInvalidCatalogRebuildsEmpty(smokeRoot / "InvalidProject");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected smoke root cleanup after test");

	std::cout << "AssetLibrarySmoke passed" << std::endl;
	return 0;
}
