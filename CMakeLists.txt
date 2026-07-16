cmake_minimum_required(VERSION 3.20)
project(GameEngineDEV LANGUAGES CXX)

# The engine is written against C++23, which every supported toolchain (GCC 13+,
# Clang 16+, MSVC v19.3x) implements. We deliberately do NOT force C++26: several
# CMake + MSVC combinations on CI do not advertise a CXX26 dialect flag yet, which
# makes try_compile checks (e.g. FindThreads, pulled in transitively by the OpenXR
# package) fail before the engine is even built.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable multi-processor compilation for MSVC
if(MSVC)
    add_compile_options(/MP /W4 /WX)
    # windows.h defines min()/max() macros that clobber std::min/std::max (used in
    # e.g. AudioEngine.cpp). NOMINMAX disables them.
    add_compile_definitions(NOMINMAX)
endif()

# Output directory for binary
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ---------------------------------------------------------------------------
# VREngineCore: the engine as a static library. Game code lives outside, in
# Samples/, and links against this target. Everything under Source/ must stay
# game-agnostic.
#
# The platform-independent portion (math, physics, audio DSP, glTF, UI, core)
# builds everywhere; the native VR layer (DirectX 12 + OpenXR + WASAPI + Win32)
# is appended on Windows only.
# ---------------------------------------------------------------------------
set(ENGINE_SOURCES
    Source/Core/Component.h
    Source/Core/GameObject.h
    Source/Core/IGame.h
    Source/Core/Timer.cpp
    Source/Core/Timer.h
    Source/Core/Logger.cpp
    Source/Core/Logger.h
    Source/Math/Vector3.h
    Source/Math/Matrix4x4.h
    Source/Math/Matrix4x4.cpp
    Source/Math/Quaternion.h
    Source/Math/Quaternion.cpp
    Source/Audio/HRTFFilter.cpp
    Source/Audio/HRTFFilter.h
    Source/Audio/AudioSource.cpp
    Source/Audio/AudioSource.h
    Source/Audio/AudioEngine.cpp
    Source/Audio/AudioEngine.h
    Source/Physics/Rigidbody.cpp
    Source/Physics/Rigidbody.h
    Source/Physics/RigidbodyComponent.h
    Source/Physics/Colliders.cpp
    Source/Physics/Colliders.h
    Source/Physics/GJK_EPA.cpp
    Source/Physics/GJK_EPA.h
    Source/Physics/PhysicsEngine.cpp
    Source/Physics/PhysicsEngine.h
    Source/Asset/AssetLoader.cpp
    Source/Asset/AssetLoader.h
    Source/Asset/TextureLoader.cpp
    Source/Asset/TextureLoader.h
    Source/UI/ImmediateUI.cpp
    Source/UI/ImmediateUI.h
)

if(WIN32)
    list(APPEND ENGINE_SOURCES
        Source/Platform/Win32Window.cpp
        Source/Platform/Win32Window.h
        Source/Platform/OpenXRManager.cpp
        Source/Platform/OpenXRManager.h
        Source/Renderer/ConstantBuffer.h
        Source/Renderer/PipelineState.cpp
        Source/Renderer/PipelineState.h
        Source/Renderer/DX12Renderer.cpp
        Source/Renderer/DX12Renderer.h
        Source/UI/FontRenderer.cpp
        Source/UI/FontRenderer.h
        Source/Core/Engine.cpp
        Source/Core/Engine.h
    )
endif()

add_library(VREngineCore STATIC ${ENGINE_SOURCES})
target_include_directories(VREngineCore PUBLIC Source)

if(WIN32)
    # OpenXR loader (provided via vcpkg manifest, see vcpkg.json). Required to
    # resolve the ~48 xr* call sites in Source/Platform/OpenXRManager.cpp.
    find_package(OpenXR CONFIG REQUIRED)
    target_link_libraries(VREngineCore PUBLIC
        d3d12 dxgi d3dcompiler
        OpenXR::openxr_loader
    )
else()
    find_package(Threads REQUIRED)
    target_link_libraries(VREngineCore PUBLIC Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# Sample game: "Special Target Shooter". Demonstrates how a game links against
# VREngineCore. The executable keeps the historical VREngine name because the
# E2E runner (Tests/run_tests.py) and CI drive it by that name.
#
# TestMain.cpp provides the CLI test harness entry point (main) used by the E2E
# suite on every platform; on Windows the VR app entry point (WinMain) is also
# compiled in, but the console subsystem selects main.
# ---------------------------------------------------------------------------
set(SAMPLE_SOURCES
    Samples/TargetShooter/GameWorld.cpp
    Samples/TargetShooter/GameWorld.h
    Samples/TargetShooter/TargetObject.cpp
    Samples/TargetShooter/TargetObject.h
    Samples/TargetShooter/TargetRotator.cpp
    Samples/TargetShooter/TargetRotator.h
    Samples/TargetShooter/TestMain.cpp
)

if(WIN32)
    list(APPEND SAMPLE_SOURCES
        Samples/TargetShooter/PlayerController.cpp
        Samples/TargetShooter/PlayerController.h
        Samples/TargetShooter/Main.cpp
    )
endif()

add_executable(VREngine ${SAMPLE_SOURCES})
target_link_libraries(VREngine PRIVATE VREngineCore)
