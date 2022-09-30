-- This is in a separate file so we can exlude it in linux builds
-- Older versions of premake can't handle "None" type
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