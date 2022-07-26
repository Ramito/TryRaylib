
function platform_defines()
    defines{"PLATFORM_DESKTOP"}
    defines{"GRAPHICS_API_OPENGL_43"}
    filter{}
end

function get_raylib_dir()
    return "lib/raylib"
end

function link_raylib()
    links {"raylib"}

    raylib_dir = get_raylib_dir();
    includedirs {"../" .. raylib_dir .. "/src" }
	includedirs {"../" .. raylib_dir .."/src/external" }
	includedirs {"../" .. raylib_dir .."/src/external/glfw/include" }
    platform_defines()

    filter "action:vs*"
        defines{"_WINSOCK_DEPRECATED_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS"}
        dependson {"raylib"}
        links {"raylib.lib"}
        characterset ("MBCS")

    filter "system:windows"
        defines{"_WIN32"}
        links {"winmm", "kernel32", "opengl32", "gdi32"}
        libdirs {"../_bin/%{cfg.buildcfg}"}

    filter "system:linux"
        links {"pthread", "GL", "m", "dl", "rt", "X11"}

    filter "system:macosx"
        links {"OpenGL.framework", "Cocoa.framework", "IOKit.framework", "CoreFoundation.framework", "CoreAudio.framework", "CoreVideo.framework"}

    filter{}
end

function include_raylib()
    raylib_dir = get_raylib_dir();
    includedirs {"../" .. raylib_dir .."/src" }
	includedirs {"../" .. raylib_dir .."/src/external" }
	includedirs {"../" .. raylib_dir .."/src/external/glfw/include" }
    platform_defines()

    filter "action:vs*"
        defines{"_WINSOCK_DEPRECATED_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS"}

    filter{}
end

project "raylib"
    kind "StaticLib"

    platform_defines()

    location ".build"
    language "C++"
    targetdir "_bin/%{cfg.buildcfg}"

    filter "action:vs*"
        defines{"_WINSOCK_DEPRECATED_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS"}
        characterset ("MBCS")

    filter{}

    raylib_dir = get_raylib_dir();
    print ("Using raylib dir " .. raylib_dir);
    includedirs {raylib_dir .. "/src", raylib_dir .. "/src/external/glfw/include" }
    vpaths
    {
        ["Header Files"] = { raylib_dir .. "/src/**.h"},
        ["Source Files/*"] = { raylib_dir .. "/src/**.c"},
    }
    files {raylib_dir .. "/src/*.h", raylib_dir .. "/src/*.c"}
    filter { "system:macosx", "files:" .. raylib_dir .. "/src/rglfw.c" }
        compileas "Objective-C"

    filter{}