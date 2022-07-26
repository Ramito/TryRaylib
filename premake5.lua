local BUILD_DIR = path.join(".build", _ACTION)
if _OPTIONS["cc"] ~= nil then
	BUILD_DIR = BUILD_DIR .. "_" .. _OPTIONS["cc"]
end

local SRC_DIR = "src"
local LIB_DIR = "lib"
local ENTT_DIR = path.join(LIB_DIR, "entt")
local RAYLIB_DIR = path.join(LIB_DIR, "raylib")

solution "RayTest"
	location(BUILD_DIR)
	startproject "RayTest"
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
	
project "Game"
	kind "ConsoleApp"
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
		SRC_DIR,
		path.join(ENTT_DIR, "src"),
		path.join(RAYLIB_DIR, "src")
	}

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

include ("raylib_premake5.lua")
