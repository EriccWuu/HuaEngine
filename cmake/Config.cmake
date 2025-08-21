# CMake configuration for different build types
# This file handles the Dist configuration that isn't standard in CMake

# Add custom Dist configuration
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Dist" CACHE STRING "" FORCE)

# Set properties for Dist configuration
set(CMAKE_CXX_FLAGS_DIST "${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_DIST "${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_EXE_LINKER_FLAGS_DIST "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set(CMAKE_MODULE_LINKER_FLAGS_DIST "${CMAKE_MODULE_LINKER_FLAGS_RELEASE}")
set(CMAKE_SHARED_LINKER_FLAGS_DIST "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
set(CMAKE_STATIC_LINKER_FLAGS_DIST "${CMAKE_STATIC_LINKER_FLAGS_RELEASE}")

# Disable debug symbols for Dist builds
if(MSVC)
    set(CMAKE_CXX_FLAGS_DIST "${CMAKE_CXX_FLAGS_DIST} /DNDEBUG /Zi-")
    set(CMAKE_C_FLAGS_DIST "${CMAKE_C_FLAGS_DIST} /DNDEBUG /Zi-")
else()
    set(CMAKE_CXX_FLAGS_DIST "${CMAKE_CXX_FLAGS_DIST} -DNDEBUG -g0")
    set(CMAKE_C_FLAGS_DIST "${CMAKE_C_FLAGS_DIST} -DNDEBUG -g0")
endif()

# Set default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "Dist")
endif()
