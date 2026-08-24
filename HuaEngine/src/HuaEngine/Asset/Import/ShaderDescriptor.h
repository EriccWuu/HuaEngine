#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"

namespace HE {
	struct ShaderDescriptorStage {
		std::string Entry;
		std::string Profile;
	};

	struct ShaderDescriptor {
		std::string Name;
		std::filesystem::path Source;
		ShaderDescriptorStage Vertex;
		ShaderDescriptorStage Fragment;
		std::vector<Rendering::ShaderParameterMetadata> Parameters;
	};

	ResultEnvelope LoadShaderDescriptor(const std::filesystem::path& path, ShaderDescriptor& outDescriptor);
	ResultEnvelope SaveShaderDescriptor(const std::filesystem::path& path, const ShaderDescriptor& descriptor);
}
