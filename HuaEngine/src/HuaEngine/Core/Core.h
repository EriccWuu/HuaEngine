#pragma once

#ifdef HE_PLATFORM_WINDOWS
	#ifdef HE_BUILD_DLL
		#define ENGINE_API __declspec(dllexport)
	#else
		#define ENGINE_API __declspec(dllimport)
	#endif
#else
	#error Hua Engine only support windows !	
#endif

#define BIT(x) (1 << x)
