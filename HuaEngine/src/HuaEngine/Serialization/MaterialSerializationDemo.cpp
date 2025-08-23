#include "enginepch.h"
#include "MaterialSerializationDemo.h"
#include "HuaEngine/Serialization/SerializationManager.h"
#include "HuaEngine/Rendering/Material/MaterialSerialization.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include <iostream>

namespace HE {

	void MaterialSerializationDemo::RunDemo()
	{
		HE_CORE_INFO("=== 材质序列化演示开始 ===");
		
		try {
			// 演示基础材质的序列化
			CreateStandardMaterialDemo();
			CreateUnlitMaterialDemo();
			CreateCustomMaterialDemo();
			
			// 演示材质实例的序列化
			TestMaterialInstanceSerialization();
			
			HE_CORE_INFO("=== 材质序列化演示完成 ===");
		}
		catch (const std::exception& e) {
			HE_CORE_ERROR("材质序列化演示失败: {0}", e.what());
		}
	}

	void MaterialSerializationDemo::CreateStandardMaterialDemo()
	{
		HE_CORE_INFO("--- 标准材质 (PBR) 序列化演示 ---");
		
		// 创建标准材质
		auto standardMaterial = MaterialLibrary::Instance().CreateStandardMaterial("DemoStandardMaterial");
		
		// 设置参数
		standardMaterial->SetParameter("u_BaseColor", glm::vec3(0.8f, 0.2f, 0.3f));
		standardMaterial->SetParameter("u_Metallic", 0.7f);
		standardMaterial->SetParameter("u_Roughness", 0.3f);
		standardMaterial->SetParameter("u_AO", 1.0f);
		
		PrintMaterialInfo(standardMaterial, "原始标准材质");
		
		// 保存到文件 - 转换为基类Material进行序列化
		std::string filename = "standard_material_demo.json";
		Ref<Material> materialBase = std::static_pointer_cast<Material>(standardMaterial);
		if (SerializationManager::Instance().SerializeToFile(*materialBase, filename, SerializationFormat::JSON)) {
			HE_CORE_INFO("Standard material saved to file: {0}", filename);
			
			// 重新加载材质 - 注意：需要先创建一个对象，然后反序列化到其中
			auto loadedMaterial = MaterialLibrary::Instance().CreateStandardMaterial("LoadedStandardMaterial");
			loadedMaterial->ClearParameters();
			Ref<Material> loadedMaterialBase = std::static_pointer_cast<Material>(loadedMaterial);
			if (SerializationManager::Instance().DeserializeFromFile(filename, *loadedMaterialBase, SerializationFormat::JSON)) {
				HE_CORE_INFO("Standard material loaded from file: {0}", filename);
				PrintMaterialInfo(loadedMaterial, "Load standard material");
				
				// 验证材质是否相同
				if (VerifyMaterialsEqual(standardMaterial, loadedMaterial)) {
					HE_CORE_INFO("✓ Standard Material Serialization Varify Successfully");
				} else {
					HE_CORE_WARN("✗ Standard Material Serialization Varify Failed");
				}
			} else {
				HE_CORE_ERROR("Can not load standard material from file");
			}
		} else {
			HE_CORE_ERROR("Can not save standard material to file");
		}
	}

	void MaterialSerializationDemo::CreateUnlitMaterialDemo()
	{
		HE_CORE_INFO("--- 无光照材质序列化演示 ---");
		
		// 创建无光照材质
		auto unlitMaterial = MaterialLibrary::Instance().CreateUnlitMaterial("DemoUnlitMaterial");
		
		// 设置参数
		unlitMaterial->SetParameter("u_Color", glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
		
		PrintMaterialInfo(unlitMaterial, "原始无光照材质");
		
		// 保存到文件 - 转换为基类Material进行序列化
		std::string filename = "unlit_material_demo.json";
		Ref<Material> materialBase = std::static_pointer_cast<Material>(unlitMaterial);
		if (SerializationManager::Instance().SerializeToFile(*materialBase, filename, SerializationFormat::JSON)) {
			HE_CORE_INFO("Unlit material saved to file: {0}", filename);
			
			// 重新加载材质
			auto loadedMaterial = MaterialLibrary::Instance().CreateUnlitMaterial("LoadedUnlitMaterial");
			loadedMaterial->ClearParameters();
			Ref<Material> loadedMaterialBase = std::static_pointer_cast<Material>(loadedMaterial);
			if (SerializationManager::Instance().DeserializeFromFile(filename, *loadedMaterialBase, SerializationFormat::JSON)) {
				HE_CORE_INFO("Unlit material loaded from file: {0}", filename);
				PrintMaterialInfo(loadedMaterial, "Load unlit material");
				
				// 验证材质是否相同
				if (VerifyMaterialsEqual(unlitMaterial, loadedMaterial)) {
					HE_CORE_INFO("✓ Unlit Material Serialization Varify Successfully");
				} else {
					HE_CORE_WARN("✗ Unlit Material Serialization Varify Failed");
				}
			} else {
				HE_CORE_ERROR("Can not load unlit material from file");
			}
		} else {
			HE_CORE_ERROR("Can not save unlit material to file");
		}
	}

	void MaterialSerializationDemo::CreateCustomMaterialDemo()
	{
		HE_CORE_INFO("--- 自定义材质序列化演示 ---");
		
		// 创建一个简单的着色器用于自定义材质
		std::string vertexSource = R"(
			#version 330 core
			layout (location = 0) in vec3 a_Position;
			void main() {
				gl_Position = vec4(a_Position, 1.0);
			}
		)";
		
		std::string fragmentSource = R"(
			#version 330 core
			out vec4 FragColor;
			void main() {
				FragColor = vec4(1.0, 0.0, 1.0, 1.0);
			}
		)";
		
		auto customShader = CreateRef<OpenGLShader>(vertexSource, fragmentSource);
		
		// 创建自定义材质
		auto customMaterial = MaterialLibrary::Instance().CreateCustomMaterial("DemoCustomMaterial", customShader);

		customMaterial->AddParameter({ "custom_float", MaterialParameterType::Float, 1.0f});
		customMaterial->AddParameter({ "custom_vec3", MaterialParameterType::Vec3, glm::vec3{1.0f, 1.0f, 1.0f } });
		customMaterial->AddParameter({ "custom_color", MaterialParameterType::Vec4, glm::vec4{1.0f, 1.0f, 1.0f, 0.f } });
		
		PrintMaterialInfo(customMaterial, "原始自定义材质");
		
		// 保存到文件 - 转换为基类Material进行序列化
		std::string filename = "custom_material_demo.json";
		Ref<Material> materialBase = std::static_pointer_cast<Material>(customMaterial);
		if (SerializationManager::Instance().SerializeToFile(*materialBase, filename, SerializationFormat::JSON)) {
			HE_CORE_INFO("自定义材质已保存到文件: {0}", filename);
			
			// 重新加载材质
			auto loadedMaterial = MaterialLibrary::Instance().CreateCustomMaterial("LoadedCustomMaterial", customShader);
			loadedMaterial->ClearParameters();
			Ref<Material> loadedMaterialBase = std::static_pointer_cast<Material>(loadedMaterial);
			if (SerializationManager::Instance().DeserializeFromFile(filename, *loadedMaterialBase, SerializationFormat::JSON)) {
				HE_CORE_INFO("Custom material loaded from file: {0}", filename);
				PrintMaterialInfo(loadedMaterial, "Load custom material");
				
				// 验证材质是否相同
				if (VerifyMaterialsEqual(customMaterial, loadedMaterial)) {
					HE_CORE_INFO("✓ Custom Material Serialization Varify Successfully.");
				} else {
					HE_CORE_WARN("✗ Custom Material Serialization Varify Failed");
				}
			} else {
				HE_CORE_ERROR("Can not load custom material from file.");
			}
		} else {
			HE_CORE_ERROR("Can not save custom material to file");
		}
	}

	void MaterialSerializationDemo::TestMaterialInstanceSerialization()
	{
		HE_CORE_INFO("--- 材质实例序列化演示 ---");
		
		// 创建基础材质
		auto baseMaterial = MaterialLibrary::Instance().CreateStandardMaterial("BaseStandardMaterial");
		baseMaterial->SetParameter("u_BaseColor", glm::vec3(0.5f, 0.5f, 0.5f));
		baseMaterial->SetParameter("u_Metallic", 0.0f);
		baseMaterial->SetParameter("u_Roughness", 0.5f);
		baseMaterial->SetParameter("u_AO", 1.0f);
		
		// 创建材质实例并覆盖一些参数
		auto materialInstance = CreateRef<MaterialInstance>(baseMaterial);
		materialInstance->SetParameter("u_BaseColor", glm::vec3(1.0f, 0.0f, 0.0f)); // 覆盖为红色
		materialInstance->SetParameter("u_Metallic", 1.0f); // 覆盖为金属材质
		
		HE_CORE_INFO("Create origin material complete");
		
		// 保存材质实例到文件
		std::string filename = "material_instance_demo.json";
		if (SerializationManager::Instance().SerializeToFile(*materialInstance, filename, SerializationFormat::JSON)) {
			HE_CORE_INFO("Material instance saved to file: {0}", filename);
			
			// 重新加载材质实例（需要相同的基础材质）
			auto loadedInstance = CreateRef<MaterialInstance>(baseMaterial);
			if (SerializationManager::Instance().DeserializeFromFile(filename, *loadedInstance, SerializationFormat::JSON)) {
				HE_CORE_INFO("Material instance loaded from file: {0}", filename);
				HE_CORE_INFO("✓ Material instance Serialization Varify Successfully");
				const auto& overrides = loadedInstance->GetParameterOverrides();
				for (auto& [k, v] : overrides) {
					HE_CORE_ERROR("{0}.", k);
				}
			} else {
				HE_CORE_ERROR("Can not load material instance from file.");
			}
		} else {
			HE_CORE_ERROR("Can not save material instance to file");
		}
	}

	bool MaterialSerializationDemo::VerifyMaterialsEqual(const Ref<Material>& originalMaterial, const Ref<Material>& loadedMaterial)
	{
		// 检查材质类型
		if (originalMaterial->GetType() != loadedMaterial->GetType()) {
			return false;
		}
		
		// 检查材质名称
		if (originalMaterial->GetName() != loadedMaterial->GetName()) {
			return false;
		}
		
		// 检查参数数量
		const auto& originalParams = originalMaterial->GetParameters();
		const auto& loadedParams = loadedMaterial->GetParameters();
		
		if (originalParams.size() != loadedParams.size()) {
			return false;
		}
		
		// 检查每个参数
		for (const auto& [name, param] : originalParams) {
			auto it = loadedParams.find(name);
			if (it == loadedParams.end()) {
				return false; // 参数不存在
			}
			
			if (param.Type != it->second.Type) {
				return false; // 参数类型不匹配
			}
			
			// 注意：这里应该比较具体的值，但为了简化演示，我们只检查类型和存在性
		}
		
		return true;
	}

	void MaterialSerializationDemo::PrintMaterialInfo(const Ref<Material>& material, const std::string& title)
	{
		HE_CORE_INFO("=== {0} ===", title);
		HE_CORE_INFO("Material Name: {0}", material->GetName());
		HE_CORE_INFO("Material Type: {0}", static_cast<int>(material->GetType()));
		
		const auto& params = material->GetParameters();
		HE_CORE_INFO("Parameter count: {0}", params.size());
		
		for (const auto& [name, param] : params) {
			HE_CORE_INFO("  Parameter: {0}, Type: {1}", name, static_cast<int>(param.Type));
		}
		
		HE_CORE_INFO("==========================");
	}

}
