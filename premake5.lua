local BUILD_DIR = path.join(".build", _ACTION)
if _OPTIONS["cc"] ~= nil then
	BUILD_DIR = BUILD_DIR .. "_" .. _OPTIONS["cc"]
end

local TARGET_DIR = ".bin/%{cfg.buildcfg}"

local SRC_DIR = "src"
local LIB_DIR = "lib"
local ENTT_DIR = path.join(LIB_DIR, "entt")
local RAYLIB_DIR = path.join(LIB_DIR, "raylib")

solution "RayTest"
	location(BUILD_DIR)
	targetdir(TARGET_DIR)
	startproject "Game"
	debugdir(".")
	configurations { "Release", "Debug" }
	platforms "x86_64"
	filter "configurations:Release"
		defines "NDEBUG"
		optimize "Full"
	filter "configurations:Debug*"
		defines "_DEBUG"
		optimize "Debug"
		symbols "On"
	filter "platforms:x86_64"
		architecture "x86_64"
	filter {}
	
project "Game"
	kind "WindowedApp"
	entrypoint "mainCRTStartup"
	language "C++"
	cppdialect "C++20"
	exceptionhandling "Off"
	rtti "Off"
	files {
		"src/**.cpp",
		"src/**.h",
	}

	includedirs
	{
        SRC_DIR
	}

	includedirs {ENTT_DIR .. "/src"}

    links {"raylib"}
	includedirs {RAYLIB_DIR .. "/src", RAYLIB_DIR .. "/src/external/glfw/include" }
	defines{"PLATFORM_DESKTOP"}
    defines{"GRAPHICS_API_OPENGL_43"}
	defines{"_WINSOCK_DEPRECATED_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS"}
	dependson {"raylib"}
	links {"raylib.lib"}
	characterset ("MBCS")
	defines{"_WIN32"}
	links {"winmm", "kernel32", "opengl32", "gdi32"}
	libdirs {TARGET_DIR}

project "entt"
	kind "None"
	language "C++"
	cppdialect "C++17"
	exceptionhandling "Off"
	rtti "Off"
	files
	{
		path.join(ENTT_DIR, "src/**.h"),
		path.join(ENTT_DIR, "src/**.hpp")
	}

project "raylib"
    kind "StaticLib"

    defines{"PLATFORM_DESKTOP"}
    defines{"GRAPHICS_API_OPENGL_43"}

	location(BUILD_DIR)
    language "C++"

    filter "action:vs*"
        defines{"_WINSOCK_DEPRECATED_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS"}
        characterset ("MBCS")

    filter{}

    defines{"_WIN32"}
    links {"winmm", "kernel32", "opengl32", "gdi32"}
    
    print ("Using raylib dir " .. RAYLIB_DIR);
    includedirs {RAYLIB_DIR .. "/src", RAYLIB_DIR .. "/src/external/glfw/include" }
    vpaths
    {
        ["Header Files"] = { RAYLIB_DIR .. "/src/**.h"},
        ["Source Files/*"] = { RAYLIB_DIR .. "/src/**.c"},
    }
    files {RAYLIB_DIR .. "/src/*.h", RAYLIB_DIR .. "/src/*.c"}
