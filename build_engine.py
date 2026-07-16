import os
import sys
import subprocess
import shutil

def find_vs_path():
    # Standard installation paths for VS 2022 and VS 2019
    paths = [
        r"C:\Program Files\Microsoft Visual Studio\2022\Community",
        r"C:\Program Files\Microsoft Visual Studio\2022\Professional",
        r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
        r"C:\Program Files\Microsoft Visual Studio\2022\BuildTools",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools",
    ]
    for p in paths:
        if os.path.exists(p):
            return p
    return None

def main():
    vs_path = find_vs_path()
    if not vs_path:
        print("Visual Studio installation not found in typical locations.")
        sys.exit(1)
    
    print(f"Found Visual Studio at: {vs_path}")
    
    # Check for bundled CMake
    cmake_path = os.path.join(vs_path, "Common7", "IDE", "CommonExtensions", "Microsoft", "CMake", "CMake", "bin", "cmake.exe")
    vcvars_path = os.path.join(vs_path, "VC", "Auxiliary", "Build", "vcvarsall.bat")
    
    # Try using CMake if available
    if os.path.exists(cmake_path):
        print(f"Found bundled CMake at: {cmake_path}")
        # Run CMake configure
        configure_cmd = [cmake_path, "-B", "Build", "-S", "."]
        print(f"Running: {' '.join(configure_cmd)}")
        res = subprocess.run(configure_cmd)
        if res.returncode != 0:
            print("CMake configuration failed.")
            sys.exit(1)
            
        # Run CMake build
        build_cmd = [cmake_path, "--build", "Build", "--config", "Release"]
        print(f"Running: {' '.join(build_cmd)}")
        res = subprocess.run(build_cmd)
        if res.returncode != 0:
            print("CMake build failed.")
            sys.exit(1)
            
        # Copy executable to root
        exe_src_options = [
            os.path.join("Build", "Release", "VREngine.exe"),
            os.path.join("Build", "VREngine.exe")
        ]
        exe_dest = "VREngine.exe"
        copied = False
        for opt in exe_src_options:
            if os.path.exists(opt):
                shutil.copy(opt, exe_dest)
                print(f"Successfully compiled and copied VREngine.exe to root from {opt}.")
                copied = True
                break
        if not copied:
            print("Could not find compiled VREngine.exe in Build output directories.")
            sys.exit(1)
            
    elif os.path.exists(vcvars_path):
        print(f"CMake not found, but found vcvarsall.bat at: {vcvars_path}")
        print("Falling back to direct compilation using cl.exe...")
        
        # Build command using cl.exe inside vcvars environment
        source_files = [
            # Engine (VREngineCore sources)
            r"Source\Core\Timer.cpp",
            r"Source\Core\Logger.cpp",
            r"Source\Core\Engine.cpp",
            r"Source\Math\Matrix4x4.cpp",
            r"Source\Math\Quaternion.cpp",
            r"Source\Platform\Win32Window.cpp",
            r"Source\Platform\OpenXRManager.cpp",
            r"Source\Renderer\PipelineState.cpp",
            r"Source\Renderer\DX12Renderer.cpp",
            r"Source\Audio\HRTFFilter.cpp",
            r"Source\Audio\AudioSource.cpp",
            r"Source\Audio\AudioEngine.cpp",
            r"Source\Physics\Rigidbody.cpp",
            r"Source\Physics\Colliders.cpp",
            r"Source\Physics\GJK_EPA.cpp",
            r"Source\Physics\PhysicsEngine.cpp",
            r"Source\Asset\AssetLoader.cpp",
            r"Source\Asset\TextureLoader.cpp",
            r"Source\UI\FontRenderer.cpp",
            r"Source\UI\ImmediateUI.cpp",
            # Sample game (Samples/TargetShooter)
            r"Samples\TargetShooter\TargetObject.cpp",
            r"Samples\TargetShooter\TargetRotator.cpp",
            r"Samples\TargetShooter\PlayerController.cpp",
            r"Samples\TargetShooter\GameWorld.cpp",
            r"Samples\TargetShooter\TestMain.cpp",
            r"Samples\TargetShooter\Main.cpp"
        ]

        sources_str = " ".join(source_files)
        compile_cmd = f'cmd.exe /c "call \\"{vcvars_path}\\" x64 && cl.exe /EHsc /std:c++latest /O2 /ISource {sources_str} d3d12.lib dxgi.lib d3dcompiler.lib /Fe:VREngine.exe"'
        print(f"Running compilation command:\n{compile_cmd}")
        
        res = subprocess.run(compile_cmd, shell=True)
        if res.returncode != 0:
            print("Direct cl.exe compilation failed.")
            sys.exit(1)
        print("Successfully compiled VREngine.exe at root using direct cl.exe.")
    else:
        print("Neither CMake nor vcvarsall.bat could be located.")
        sys.exit(1)

if __name__ == "__main__":
    main()
