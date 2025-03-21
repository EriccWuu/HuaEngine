#pragma once

#ifdef HE_PLATFORM_WINDOWS
	#ifdef HE_DYNAMIC_BUILD
		#ifdef HE_BUILD_DLL
			#define ENGINE_API __declspec(dllexport)
		#else
			#define ENGINE_API __declspec(dllimport)
		#endif
	#else
		#define ENGINE_API 
	#endif
#else
	#error Hua Engine only support windows !	
#endif

#define BIT(x) (1 << x)

#define BIND_EVENT_FUNC(x) std::bind(&x, this, std::placeholders::_1)
