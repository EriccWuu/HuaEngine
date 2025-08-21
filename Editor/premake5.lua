project "Editor"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "src/**.h",
        "src/**.cpp"
    }

    includedirs
    {
        "%{wks.location}/Editor/src",
        "%{wks.location}/HuaEngine/src",
        "%{IncludeDirs.spdlog}",
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
