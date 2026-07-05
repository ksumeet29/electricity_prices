import os
import sys
import shutil
import subprocess

BUILD_DIR = "build"

# Remove existing build directory
if os.path.exists(BUILD_DIR):
    shutil.rmtree(BUILD_DIR)

# Create build directory
os.mkdir(BUILD_DIR)
os.chdir(BUILD_DIR)

result = subprocess.run(
    [
        "cmake",
        "..",
        "-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake",
    ],
    capture_output=True,
    text=True,
)

if result.returncode != 0:
    print(result.stdout)
    print(result.stderr)
    sys.exit(result.returncode)

# Build (hide output)
result = subprocess.run(
    [
        "cmake",
        "--build",
        ".",
        "--config",
        "Release",
    ],
    capture_output=True,
    text=True,
)

if result.returncode != 0:
    print(result.stdout)
    print(result.stderr)
    sys.exit(result.returncode)

# Run executable (show output)
subprocess.run(
    [os.path.join("Release", "elpris.exe")],
    check=True,
)