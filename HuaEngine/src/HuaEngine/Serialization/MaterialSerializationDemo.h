#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Material/MaterialLibrary.h"
#include "HuaEngine/Rendering/Material/MaterialTypes.h"

namespace HE {

	/// @brief 材质序列化演示类
	/// 展示如何使用材质系统的序列化功能来保存和加载材质
	class MaterialSerializationDemo {
	public:
		/// @brief 运行材质序列化演示
		/// 创建不同类型的材质，保存到文件，然后重新加载
		static void RunDemo();

	private:
		/// @brief 创建标准材质演示
		static void CreateStandardMaterialDemo();

		/// @brief 创建无光照材质演示
		static void CreateUnlitMaterialDemo();

		/// @brief 创建自定义材质演示
		static void CreateCustomMaterialDemo();

		/// @brief 测试材质实例序列化
		static void TestMaterialInstanceSerialization();

		/// @brief 验证序列化的材质是否正确
		/// @param originalMaterial 原始材质
		/// @param loadedMaterial 从文件加载的材质
		/// @return 如果两个材质相等返回true
		static bool VerifyMaterialsEqual(const Ref<Material>& originalMaterial, const Ref<Material>& loadedMaterial);

		/// @brief 打印材质信息到控制台
		/// @param material 要打印的材质
		/// @param title 标题
		static void PrintMaterialInfo(const Ref<Material>& material, const std::string& title);
	};

}
