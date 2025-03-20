project "GLAD"
    kind "StaticLib"
    language "C"
    staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files 
    {
        "include/**.h",
        "src/**.c",
    }

    includedirs
    {
        "include"
    }

    filter "system:windows"
        systemversion "latest"
        buildoptions
        {
            "/external:W0"
        }

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

    filter "configurations:Dist"
		runtime "Release"
		optimize "on"
        symbols "off"

