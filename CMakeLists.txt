cmake_minimum_required(VERSION 3.21)
project(CGExperiment_3DViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ---------------------------------------------------------------------------
# Vendored freeglut (offline build), 32 位和 64 位都提供。
#
#   third_party/freeglut/include/GL/        头文件（架构无关）
#   third_party/freeglut/lib/Win32|x64/     导入库
#   third_party/freeglut/bin/Win32|x64/     DLL
#
# Deliberately NO find_package(GLUT) and NO CMAKE_PREFIX_PATH: FindGLUT on
# Windows searches PATH/registry and would silently latch onto whatever copy
# happens to be installed (this machine has an unrelated one in Anaconda),
# making the build machine-specific and defeating the point of vendoring.
# ---------------------------------------------------------------------------
set(FREEGLUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/freeglut")

# 依赖按平台分目录：lib/Win32、lib/x64，bin 同理。
# 用指针宽度判断而不是 CMAKE_GENERATOR_PLATFORM —— 后者只在 VS 生成器下有值，
# 换 NMake / Ninja 就是空的。
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(FREEGLUT_PLATFORM "x64")
else()
    set(FREEGLUT_PLATFORM "Win32")
endif()

set(FREEGLUT_LIB "${FREEGLUT_DIR}/lib/${FREEGLUT_PLATFORM}/freeglut.lib")
set(FREEGLUT_DLL "${FREEGLUT_DIR}/bin/${FREEGLUT_PLATFORM}/freeglut.dll")

if(NOT EXISTS "${FREEGLUT_LIB}")
    message(FATAL_ERROR
        "\n找不到 ${FREEGLUT_PLATFORM} 版的 freeglut.lib：\n  ${FREEGLUT_LIB}\n")
endif()

add_library(freeglut_vendored SHARED IMPORTED GLOBAL)
set_target_properties(freeglut_vendored PROPERTIES
    IMPORTED_IMPLIB               "${FREEGLUT_LIB}"
    IMPORTED_LOCATION             "${FREEGLUT_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${FREEGLUT_DIR}/include"
)

# ---------------------------------------------------------------------------
# Viewer
# ---------------------------------------------------------------------------
add_executable(viewer WIN32
    src/renderer.cpp
    src/main.cpp
    src/app.cpp
    src/input.cpp
    src/scene.cpp
    src/mesh.cpp
    src/obj_loader.cpp
    src/texture.cpp
    src/camera.cpp
    src/projection.cpp
    src/lighting.cpp
    src/hud.cpp
    src/anim.cpp
    src/forest.cpp
    src/recorder.cpp
    src/robot.cpp
    src/animals.cpp
)

set_target_properties(viewer PROPERTIES WIN32_EXECUTABLE TRUE)

target_include_directories(viewer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
)

# /utf-8 is mandatory on this zh-CN Windows: MSVC would otherwise assume the GBK
# codepage for source files, mangling the Chinese comments.
# /wd4819 suppresses the "character cannot be represented in the current code
#   page" nag -- a false positive once /utf-8 has fixed the source encoding.
# _CRT_SECURE_NO_WARNINGS silences the fopen/snprintf deprecation noise.
# /MP builds the translation units in parallel.
target_compile_options(viewer PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    $<$<CXX_COMPILER_ID:MSVC>:/W3>
    $<$<CXX_COMPILER_ID:MSVC>:/wd4819>
    $<$<CXX_COMPILER_ID:MSVC>:/MP>
)
target_compile_definitions(viewer PRIVATE _CRT_SECURE_NO_WARNINGS)

# freeglut.lib provides no WinMain.  With /SUBSYSTEM:WINDOWS the linker's default
# entry (WinMainCRTStartup) would reference WinMain and fail with LNK2019.
target_link_options(viewer PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/ENTRY:mainCRTStartup>)

# glu32 is needed for gluLookAt / gluOrtho2D / gluBuild2DMipmaps.
target_link_libraries(viewer PRIVATE freeglut_vendored opengl32 glu32)

# Place freeglut.dll beside the exe (multi-config aware under the VS generator).
add_custom_command(TARGET viewer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${FREEGLUT_DLL}"
            "$<TARGET_FILE_DIR:viewer>"
    VERBATIM
)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME math_camera COMMAND viewer --selftest)
    add_executable(obj_loader_test tests/obj_loader_test.cpp src/obj_loader.cpp)
    target_include_directories(obj_loader_test PRIVATE src "${FREEGLUT_DIR}/include")
    target_compile_options(obj_loader_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_compile_definitions(obj_loader_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(obj_loader_test PRIVATE freeglut_vendored opengl32 glu32)
    add_test(NAME obj_loader COMMAND obj_loader_test)
endif()
