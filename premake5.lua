workspace "HuaEngine"
    architecture "x64"
    
    configurations 
    {
        "Debug",
        "Release",
        "Dist"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "HuaEngine"
    location "HuaEngine"
    kind "SharedLib"
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs
    {
        "Dependencies/spdlog/include"
    }

    buildoptions
    {
        "/utf-8"
    }

    filter "system:windows"
        cppdialect "C++17"
        staticruntime "On"
        systemversion "10.0.19041.0"
    
        defines
        {
            "HE_PLATFORM_WINDOWS",
            "HE_BUILD_DLL"
        }

        postbuildcommands
        {
            "{MKDIR} ../bin/%{outputdir}/Sandbox",
            "{COPY} %{cfg.targetdir}/%{prj.name}.dll ../bin/%{outputdir}/Sandbox/"
        }

    filter "configurations:Debug"
        defines "HE_DEBUG"
        symbols "On"

    filter "configurations:Release"
        defines "HE_RELEASE"
        optimize "On"
    
    filter "configurations:Dist"
        defines "HE_DIST"
        optimize "On"

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs
    {
        "Dependencies/spdlog/include",
        "HuaEngine/src"
    }

    links
    {
        "HuaEngine"
    }

    buildoptions
    {
        "/utf-8"
    }

    filter "system:windows"
        cppdialect "C++17"
        staticruntime "On"
        systemversion "10.0.19041.0"

        defines
        {
            "HE_PLATFORM_WINDOWS"
        }

    filter "configurations:Debug"
        defines "HE_DEBUG"
        symbols "On"

    filter "configurations:Release"
        defines "HE_RELEASE"
        optimize "On"
    
    filter "configurations:Dist"
        defines "HE_DIST"
        optimize "On"
