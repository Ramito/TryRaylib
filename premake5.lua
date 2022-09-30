local BUILD_DIR = path.join(".build", _ACTION)
if _OPTIONS["cc"] ~= nil then
	BUILD_DIR = BUILD_DIR .. "_" .. _OPTIONS["cc"]
end

local TARGET_DIR = ".bin/%{cfg.buildcfg}"

local SRC_DIR = "src"
local LIB_DIR = "lib"
local ENTT_DIR = path.join(LIB_DIR, "entt")
local RAYLIB_DIR = path.join(LIB_DIR, "raylib")
local RAYGUI_DIR = path.join(LIB_DIR, "raygui")
local TRACY_DIR = path.join(LIB_DIR, "tracy")

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
		RAYGUI_DIR .. "/src/**.h",
		RAYGUI_DIR .. "/src/**.c",
		TRACY_DIR .. "/TracyClient.cpp"
	}

	includedirs {SRC_DIR}

	includedirs {ENTT_DIR .. "/src"}

	includedirs {RAYGUI_DIR .. "/src"}
	defines("RAYGUI_IMPLEMENTATION")

	includedirs {RAYLIB_DIR .. "/src", RAYLIB_DIR .. "/src/external/glfw/include" }

	includedirs {TRACY_DIR }
	defines {"TRACY_ENABLE"}

	links {"raylib"}
	defines{"PLATFORM_DESKTOP"}
	defines{"GRAPHICS_API_OPENGL_43"}
	defines{"_WINSOCK_DEPRECATED_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS"}
	dependson {"raylib"}
	characterset ("MBCS")

	filter "system:Windows"
		links {"raylib.lib"}
		defines{"_WIN32"}
		links {"winmm", "kernel32", "opengl32", "gdi32"}

	filter{}
	libdirs {TARGET_DIR}

-- Older versions of premake can't handle "None" project types for gmake2
if _TARGET_OS ~= "linux" then
	include "entt-premake5.lua"
end

project "raylib"
    kind "StaticLib"

    defines{"PLATFORM_DESKTOP"}
    defines{"GRAPHICS_API_OPENGL_43"}

	location(BUILD_DIR)
    language "C++"

    filter "action:vs*"
        defines{"_WINSOCK_DEPRECATED_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS"}
        characterset ("MBCS")


	filter "system:Windows"
		defines{"_WIN32"}
		links {"winmm", "kernel32", "opengl32", "gdi32"}

    filter{}

    print ("Using raylib dir " .. RAYLIB_DIR);
    includedirs {RAYLIB_DIR .. "/src", RAYLIB_DIR .. "/src/external/glfw/include" }
    vpaths
    {
        ["Header Files"] = { RAYLIB_DIR .. "/src/**.h"},
        ["Source Files/*"] = { RAYLIB_DIR .. "/src/**.c"},
    }
    files {RAYLIB_DIR .. "/src/*.h", RAYLIB_DIR .. "/src/*.c"}

		
