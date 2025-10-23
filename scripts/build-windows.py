#!/usr/bin/env python3
"""
Standard-of-Iron — Windows Build Script
Verifies dependencies, sets up MSVC environment, and builds the project.

This script automates the Windows build process by:
1. Checking for required tools (CMake, Ninja, MSVC, Qt)
2. Guiding installation of missing dependencies
3. Configuring and building the project with proper MSVC setup
4. Deploying Qt dependencies
5. Copying assets and creating a distributable package

Usage:
    python scripts/build-windows.py                    # Full build with checks
    python scripts/build-windows.py --skip-checks      # Skip dependency checks
    python scripts/build-windows.py --build-type Debug # Build in Debug mode
    python scripts/build-windows.py --clean            # Clean build directory first
    python scripts/build-windows.py --deploy-only      # Only deploy Qt (assumes built)
    python scripts/build-windows.py --help             # Show this help

Requirements:
    - Python 3.7+
    - CMake 3.21+
    - Ninja build system
    - Visual Studio 2019/2022 with C++ tools
    - Qt 6.6.3 or compatible (with msvc2019_64 or msvc2022_64)
"""

import argparse
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional, Tuple

# ANSI color codes for better output
class Color:
    BLUE = '\033[1;34m'
    GREEN = '\033[1;32m'
    YELLOW = '\033[1;33m'
    RED = '\033[1;31m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def info(msg: str) -> None:
    """Print info message."""
    print(f"{Color.BLUE}[i]{Color.RESET} {msg}")

def success(msg: str) -> None:
    """Print success message."""
    print(f"{Color.GREEN}[✓]{Color.RESET} {msg}")

def warning(msg: str) -> None:
    """Print warning message."""
    print(f"{Color.YELLOW}[!]{Color.RESET} {msg}")

def error(msg: str) -> None:
    """Print error message."""
    print(f"{Color.RED}[✗]{Color.RESET} {msg}")

def run_command(cmd: list, capture_output: bool = False, check: bool = True, 
                env: Optional[dict] = None) -> subprocess.CompletedProcess:
    """Run a command and optionally capture output."""
    try:
        result = subprocess.run(
            cmd,
            capture_output=capture_output,
            text=True,
            check=check,
            env=env or os.environ.copy()
        )
        return result
    except subprocess.CalledProcessError as e:
        if check:
            error(f"Command failed: {' '.join(cmd)}")
            if e.stdout:
                print(e.stdout)
            if e.stderr:
                print(e.stderr, file=sys.stderr)
            raise
        return e

def check_windows() -> None:
    """Verify we're running on Windows."""
    if platform.system() != 'Windows':
        error("This script is designed for Windows only.")
        error(f"Detected OS: {platform.system()}")
        sys.exit(1)
    success("Running on Windows")

def check_cmake() -> Tuple[bool, Optional[str]]:
    """Check if CMake is installed and get version."""
    try:
        result = run_command(['cmake', '--version'], capture_output=True)
        version_match = re.search(r'cmake version (\d+\.\d+\.\d+)', result.stdout)
        if version_match:
            version = version_match.group(1)
            major, minor, _ = map(int, version.split('.'))
            if major > 3 or (major == 3 and minor >= 21):
                success(f"CMake {version} found")
                return True, version
            else:
                warning(f"CMake {version} found but version 3.21+ required")
                return False, version
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    return False, None

def check_ninja() -> bool:
    """Check if Ninja is installed."""
    try:
        result = run_command(['ninja', '--version'], capture_output=True)
        version = result.stdout.strip()
        success(f"Ninja {version} found")
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False

def find_vswhere() -> Optional[Path]:
    """Find vswhere.exe to locate Visual Studio."""
    vswhere_path = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    if vswhere_path.exists():
        return vswhere_path
    return None

def check_msvc() -> Tuple[bool, Optional[str]]:
    """Check if MSVC is installed."""
    vswhere = find_vswhere()
    if not vswhere:
        return False, None
    
    try:
        result = run_command([
            str(vswhere),
            '-latest',
            '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
            '-property', 'installationVersion'
        ], capture_output=True)
        
        version = result.stdout.strip()
        if version:
            success(f"Visual Studio {version} with C++ tools found")
            return True, version
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    
    return False, None

def find_qt() -> Optional[Path]:
    """Try to find Qt installation."""
    # Common Qt installation locations
    possible_paths = [
        Path(r"C:\Qt"),
        Path.home() / "Qt",
        Path(os.environ.get('QT_ROOT', '')) if os.environ.get('QT_ROOT') else None,
    ]
    
    for base_path in possible_paths:
        if base_path and base_path.exists():
            # Look for Qt 6.x with msvc2019_64 or msvc2022_64
            for qt_version_dir in base_path.glob("6.*"):
                for arch_dir in qt_version_dir.glob("msvc*_64"):
                    qmake = arch_dir / "bin" / "qmake.exe"
                    if qmake.exists():
                        return arch_dir
    
    return None

def check_qt() -> Tuple[bool, Optional[Path], Optional[str]]:
    """Check if Qt is installed."""
    qt_path = find_qt()
    if qt_path:
        qmake = qt_path / "bin" / "qmake.exe"
        try:
            result = run_command([str(qmake), '-query', 'QT_VERSION'], capture_output=True)
            version = result.stdout.strip()
            success(f"Qt {version} found at {qt_path}")
            return True, qt_path, version
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass
    
    return False, None, None

def setup_msvc_environment() -> dict:
    """Set up MSVC environment variables."""
    info("Setting up MSVC environment...")
    
    vswhere = find_vswhere()
    if not vswhere:
        error("Cannot find vswhere.exe")
        sys.exit(1)
    
    # Find Visual Studio installation path
    result = run_command([
        str(vswhere),
        '-latest',
        '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        '-property', 'installationPath'
    ], capture_output=True)
    
    vs_path = Path(result.stdout.strip())
    if not vs_path.exists():
        error("Cannot find Visual Studio installation")
        sys.exit(1)
    
    # Find vcvarsall.bat
    vcvarsall = vs_path / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
    if not vcvarsall.exists():
        error(f"Cannot find vcvarsall.bat at {vcvarsall}")
        sys.exit(1)
    
    # Run vcvarsall and capture environment
    info(f"Running vcvarsall.bat from {vs_path}")
    cmd = f'"{vcvarsall}" x64 && set'
    
    result = subprocess.run(
        cmd,
        shell=True,
        capture_output=True,
        text=True
    )
    
    # Parse environment variables
    env = os.environ.copy()
    for line in result.stdout.split('\n'):
        if '=' in line:
            key, _, value = line.partition('=')
            env[key] = value.strip()
    
    success("MSVC environment configured")
    return env

def print_installation_guide() -> None:
    """Print guide for installing missing dependencies."""
    print(f"\n{Color.BOLD}Installation Guide:{Color.RESET}\n")
    
    print(f"{Color.BOLD}1. CMake:{Color.RESET}")
    print("   Download from: https://cmake.org/download/")
    print("   Or use: winget install Kitware.CMake\n")
    
    print(f"{Color.BOLD}2. Ninja:{Color.RESET}")
    print("   Download from: https://github.com/ninja-build/ninja/releases")
    print("   Or use: winget install Ninja-build.Ninja")
    print("   Or use: choco install ninja\n")
    
    print(f"{Color.BOLD}3. Visual Studio:{Color.RESET}")
    print("   Download: https://visualstudio.microsoft.com/downloads/")
    print("   Install 'Desktop development with C++' workload")
    print("   Or use: winget install Microsoft.VisualStudio.2022.Community")
    print("           --override '--add Microsoft.VisualStudio.Workload.NativeDesktop'\n")
    
    print(f"{Color.BOLD}4. Qt 6.6.3:{Color.RESET}")
    print("   Download: https://www.qt.io/download-open-source")
    print("   Install Qt 6.6.3 with MSVC 2019 64-bit component")
    print("   Required modules: qt5compat, qtmultimedia")
    print("   Or set QT_ROOT environment variable to your Qt installation\n")

def check_dependencies() -> Tuple[bool, Optional[Path]]:
    """Check all required dependencies."""
    info("Checking dependencies...")
    print()
    
    all_ok = True
    qt_path = None
    
    cmake_ok, _ = check_cmake()
    if not cmake_ok:
        error("CMake 3.21+ not found")
        all_ok = False
    
    ninja_ok = check_ninja()
    if not ninja_ok:
        error("Ninja not found")
        all_ok = False
    
    msvc_ok, _ = check_msvc()
    if not msvc_ok:
        error("Visual Studio with C++ tools not found")
        all_ok = False
    
    qt_ok, qt_path, _ = check_qt()
    if not qt_ok:
        error("Qt 6.x with MSVC not found")
        all_ok = False
    
    print()
    
    if not all_ok:
        error("Some dependencies are missing!")
        print_installation_guide()
        return False, None
    
    success("All dependencies found!")
    return True, qt_path

def configure_project(build_dir: Path, build_type: str, qt_path: Optional[Path], 
                     msvc_env: dict) -> None:
    """Configure the project with CMake."""
    info(f"Configuring project (Build type: {build_type})...")
    
    cmake_args = [
        'cmake',
        '-S', '.',
        '-B', str(build_dir),
        '-G', 'Ninja',
        f'-DCMAKE_BUILD_TYPE={build_type}',
        '-DDEFAULT_LANG=en'
    ]
    
    if qt_path:
        cmake_args.append(f'-DCMAKE_PREFIX_PATH={qt_path}')
    
    run_command(cmake_args, env=msvc_env)
    success("Project configured")

def build_project(build_dir: Path, msvc_env: dict) -> None:
    """Build the project."""
    info("Building project...")
    
    run_command(['cmake', '--build', str(build_dir)], env=msvc_env)
    success("Project built successfully")

def deploy_qt(build_dir: Path, qt_path: Path, app_name: str, build_type: str) -> None:
    """Deploy Qt dependencies."""
    info("Deploying Qt dependencies...")
    
    app_dir = build_dir / "bin"
    exe_path = app_dir / f"{app_name}.exe"
    
    if not exe_path.exists():
        error(f"Executable not found: {exe_path}")
        sys.exit(1)
    
    windeployqt = qt_path / "bin" / "windeployqt.exe"
    if not windeployqt.exists():
        error(f"windeployqt not found at {windeployqt}")
        sys.exit(1)
    
    qml_dir = Path("ui/qml")
    
    # Map build type to windeployqt mode flag
    mode_flag = {
        "Debug": "--debug",
        "Release": "--release",
        "RelWithDebInfo": "--release",  # release DLLs + PDBs
    }[build_type]
    
    run_command([
        str(windeployqt),
        mode_flag,
        '--compiler-runtime',  # ship VC++ runtime DLLs
        '--qmldir', str(qml_dir),
        str(exe_path)
    ])
    
    success("Qt dependencies deployed")

def copy_assets(build_dir: Path) -> None:
    """Copy assets to build directory."""
    info("Copying assets...")
    
    app_dir = build_dir / "bin"
    assets_src = Path("assets")
    assets_dst = app_dir / "assets"
    
    if assets_dst.exists():
        shutil.rmtree(assets_dst)
    
    shutil.copytree(assets_src, assets_dst)
    success("Assets copied")

def create_package(build_dir: Path, build_type: str) -> Path:
    """Create distributable package."""
    info("Creating distributable package...")
    
    app_dir = build_dir / "bin"
    package_name = f"standard_of_iron-win-x64-{build_type}.zip"
    package_path = Path(package_name)
    
    if package_path.exists():
        package_path.unlink()
    
    shutil.make_archive(
        package_path.stem,
        'zip',
        app_dir
    )
    
    success(f"Package created: {package_path}")
    return package_path

def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Build Standard-of-Iron on Windows",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument(
        '--skip-checks',
        action='store_true',
        help='Skip dependency checks'
    )
    parser.add_argument(
        '--build-type',
        choices=['Debug', 'Release', 'RelWithDebInfo'],
        default='Release',
        help='CMake build type (default: Release)'
    )
    parser.add_argument(
        '--clean',
        action='store_true',
        help='Clean build directory before building'
    )
    parser.add_argument(
        '--deploy-only',
        action='store_true',
        help='Only deploy Qt dependencies (skip build)'
    )
    parser.add_argument(
        '--no-package',
        action='store_true',
        help='Skip creating distributable package'
    )
    
    args = parser.parse_args()
    
    # Check we're on Windows
    check_windows()
    
    # Repository root
    repo_root = Path(__file__).parent.parent
    os.chdir(repo_root)
    
    build_dir = Path("build")
    qt_path = None
    
    # Check dependencies unless skipped
    if not args.skip_checks and not args.deploy_only:
        deps_ok, qt_path = check_dependencies()
        if not deps_ok:
            return 1
    else:
        # Try to find Qt even if checks are skipped
        _, qt_path, _ = check_qt()
    
    # Clean build directory if requested
    if args.clean and build_dir.exists():
        info("Cleaning build directory...")
        shutil.rmtree(build_dir)
        success("Build directory cleaned")
    
    # Set up MSVC environment
    if not args.deploy_only:
        msvc_env = setup_msvc_environment()
        
        # Configure
        configure_project(build_dir, args.build_type, qt_path, msvc_env)
        
        # Build
        build_project(build_dir, msvc_env)
    
    # Deploy Qt
    if qt_path:
        deploy_qt(build_dir, qt_path, "standard_of_iron", args.build_type)
    else:
        warning("Qt path not found, skipping windeployqt")
    
    # Copy assets
    copy_assets(build_dir)
    
    # Create package
    if not args.no_package:
        package_path = create_package(build_dir, args.build_type)
        print()
        info(f"Build complete! Package available at: {package_path}")
        info(f"Executable location: {build_dir / 'bin' / 'standard_of_iron.exe'}")
    else:
        print()
        info(f"Build complete!")
        info(f"Executable location: {build_dir / 'bin' / 'standard_of_iron.exe'}")
    
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print()
        warning("Build interrupted by user")
        sys.exit(130)
    except Exception as e:
        error(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
