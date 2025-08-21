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
IncludeDirs["spdlog"] = "%{wks.location}/Dependencies/spdlog/include"
IncludeDirs["glfw"] = "%{wks.location}/Dependencies/glfw/include"
IncludeDirs["glad"] = "%{wks.location}/Dependencies/glad/include"
IncludeDirs["imgui"] = "%{wks.location}/Dependencies/imgui"
IncludeDirs["glm"] = "%{wks.location}/Dependencies/glm"
IncludeDirs["stb_image"] = "%{wks.location}/Dependencies/stb_image"
IncludeDirs["entt"] = "%{wks.location}/Dependencies/entt"

include "Dependencies/glfw"
include "Dependencies/glad"
include "Dependencies/imgui"
include "Editor"

project "HuaEngine"
    location "HuaEngine"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs
    {
        "%{prj.name}/src",
        "%{IncludeDirs.spdlog}",
        "%{IncludeDirs.glfw}",
        "%{IncludeDirs.glad}",
        "%{IncludeDirs.imgui}",
        "%{IncludeDirs.glm}",
        "%{IncludeDirs.stb_image}",
        "%{IncludeDirs.entt}"
    }

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "%{IncludeDirs.stb_image}/**.h",
        "%{IncludeDirs.stb_image}/**.cpp",
        "%{IncludeDirs.entt}/**.hpp"
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
        systemversion "latest"
    
        defines
        {
            "HE_PLATFORM_WINDOWS",
            "HE_BUILD_DLL",
            "GLFW_INCLUDE_NONE",
            "_CRT_SECURE_NO_WARNINGS"
        }
        buildoptions
        {
            "/utf-8"
        }

    filter "configurations:Debug"
        defines {
            "HE_DEBUG",
            "HE_ENABLE_ASSERTS"
        }
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines "HE_RELEASE"
        runtime "Release"
        optimize "on"
    
    filter "configurations:Dist"
        defines "HE_DIST"
        runtime "Release"
        optimize "on"

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

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
        "HuaEngine/src",
        "%{IncludeDirs.glm}",
        "%{IncludeDirs.imgui}",
        "%{IncludeDirs.stb_image}",
        "%{IncludeDirs.entt}"
    }

    links
    {
        "HuaEngine",
        "imgui"
    }

    buildoptions
    {
        "/utf-8"
    }

    filter "system:windows"
        systemversion "latest"
        defines
            "HE_PLATFORM_WINDOWS"

    filter "configurations:Debug"
        defines {
            "HE_DEBUG",
            "HE_ENABLE_ASSERTS"
        }
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines "HE_RELEASE"
        runtime "Release"
        optimize "on"
    
    filter "configurations:Dist"
        defines "HE_DIST"
        runtime "Release"
        optimize "on"
