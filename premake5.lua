workspace "HuaEngine"
    architecture "x64"
    
    configurations 
    {
        "Debug",
        "Release",
        "Dist"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDirs = {}
IncludeDirs["spdlog"] = "Dependencies/spdlog/include"
IncludeDirs["glfw"] = "Dependencies/glfw/include"
IncludeDirs["glad"] = "Dependencies/glad/include"
IncludeDirs["imgui"] = "Dependencies/imgui"

include "Dependencies/glfw"
include "Dependencies/glad"
include "Dependencies/imgui"

project "HuaEngine"
    location "HuaEngine"
    kind "SharedLib"
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs
    {
        "%{prj.name}/src",
        "%{IncludeDirs.spdlog}",
        "%{IncludeDirs.glfw}",
        "%{IncludeDirs.glad}",
        "%{IncludeDirs.imgui}"
    }

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    links
    {
        "GLFW",
        "GLAD",
        "ImGui",
        "opengl32.lib"
    }

    pchheader ("enginepch.h")
    pchsource ("%{prj.name}/src/enginepch.cpp")

    buildoptions
    {
        "/utf-8"
    }

    filter "system:windows"
        cppdialect "C++17"
        staticruntime "off"
        systemversion "latest"
    
        defines
        {
            "HE_PLATFORM_WINDOWS",
            "HE_BUILD_DLL",
            "GLFW_INCLUDE_NONE"
        }

        postbuildcommands
        {
            "{MKDIR} ../bin/%{outputdir}/Sandbox",
            "{COPY} %{cfg.targetdir}/%{prj.name}.dll ../bin/%{outputdir}/Sandbox/"
        }

    filter "configurations:Debug"
        defines {
            "HE_DEBUG",
            "HE_ENABLE_ASSERTS"
        }
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines "HE_RELEASE"
        runtime "Release"
        optimize "On"
    
    filter "configurations:Dist"
        defines "HE_DIST"
        runtime "Release"
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
        "%{IncludeDirs.spdlog}",
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
        staticruntime "off"
        systemversion "latest"

        defines
        {
            "HE_PLATFORM_WINDOWS"
        }

    filter "configurations:Debug"
        defines "HE_DEBUG"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines "HE_RELEASE"
        runtime "Release"
        optimize "On"
    
    filter "configurations:Dist"
        defines "HE_DIST"
        runtime "Release"
        optimize "On"
