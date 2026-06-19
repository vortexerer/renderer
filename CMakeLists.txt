cmake_minimum_required(VERSION 3.20)
project(GameEngineDEV LANGUAGES CXX)

# The full Windows engine targets C++26 (MSVC). The cross-platform core only
# relies on features available in C++23, so fall back to 23 on toolchains/CMake
# versions that do not yet advertise a C++26 dialect flag (e.g. GCC 13 + CMake 3.28).
if(MSVC)
    set(CMAKE_CXX_STANDARD 26)
else()
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable multi-processor compilation for MSVC
if(MSVC)
    add_compile_options(/MP /W4 /WX)
endif()

# Include directories
include_directories(Source)

# Output directory for binary
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ---------------------------------------------------------------------------
# Platform-independent engine core.
# These translation units carry no DirectX 12 / Win32 / OpenXR / WASAPI device
# dependencies (the few that touch the OS are guarded with #ifdef _WIN32) and so
# compile on any platform. They are exercised by the E2E test runner.
# ---------------------------------------------------------------------------
set(CORE_SOURCES
    Source/Core/Component.h
    Source/Core/GameObject.h
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
    Source/Game/TargetObject.cpp
    Source/Game/TargetObject.h
    Source/Game/TargetRotator.h
    Source/Game/TargetRotator.cpp
    Source/Game/GameWorld.cpp
    Source/Game/GameWorld.h
)

if(WIN32)
    # -----------------------------------------------------------------------
    # Full VR engine executable (Windows only: DirectX 12 + OpenXR + WASAPI).
    # -----------------------------------------------------------------------
    set(SOURCES
        ${CORE_SOURCES}
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
        Source/Game/PlayerController.cpp
        Source/Game/PlayerController.h
        Source/Core/Engine.cpp
        Source/Core/Engine.h
        Source/TestMain.cpp
        Source/Main.cpp
    )

    # OpenXR loader (provided via vcpkg manifest, see vcpkg.json). Required to
    # resolve the ~48 xr* call sites in Source/Platform/OpenXRManager.cpp.
    find_package(OpenXR CONFIG REQUIRED)

    add_executable(VREngine ${SOURCES})
    target_link_libraries(VREngine PRIVATE
        d3d12 dxgi d3dcompiler
        OpenXR::openxr_loader
    )
else()
    # -----------------------------------------------------------------------
    # Cross-platform core test runner. Builds the engine core together with the
    # CLI test harness (TestMain.cpp) so the E2E suite can validate the real
    # C++ engine on Linux/macOS CI instead of the Python mock.
    # -----------------------------------------------------------------------
    find_package(Threads REQUIRED)
    add_executable(VREngine ${CORE_SOURCES} Source/TestMain.cpp)
    target_link_libraries(VREngine PRIVATE Threads::Threads)
endif()
