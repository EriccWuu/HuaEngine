#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Core/Log.h"
#include <filesystem>

#ifdef HE_ENABLE_ASSERTS
	#define HE_ASSERT(check, ...) { if(!(check)) { HE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define HE_CORE_ASSERT(check, ...) { if(!(check)) { HE_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define HE_ASSERT(...)
	#define HE_CORE_ASSERT(...)
#endif