#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"

namespace HE::Rendering {
	struct DxcCompileRequest {
		std::filesystem::path SourcePath;
		std::string EntryPoint;
		std::string Profile;
		ShaderStage Stage = ShaderStage::Vertex;
		std::vector<std::filesystem::path> IncludeRoots;
	};

	struct DxcCompileOutput {
		std::vector<uint32_t> Spirv;
		std::string Disassembly;
		std::string CompilerIdentity;
		std::vector<std::string> CompileOptions;
	};

	class DxcShaderCompiler final {
	public:
		ResultEnvelope Compile(const DxcCompileRequest& request, DxcCompileOutput& output) const;
		ResultEnvelope QueryCompilerIdentity(std::string& output) const;

	private:
		[[nodiscard]] std::filesystem::path ResolveToolDirectory() const;
	};
}
