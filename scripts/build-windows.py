#!/usr/bin/env python3
"""
Standard-of-Iron — Windows Build Script
Verifies dependencies, sets up MSVC environment, and builds the project.

This script automates the Windows build process by:
1. Checking for required tools (CMake, Ninja, MSVC, Qt)
2. Auto-installing missing dependencies (optional, uses winget)
3. Guiding installation of missing dependencies
4. Configuring and building the project with proper MSVC setup
5. Deploying Qt dependencies with runtime libraries
6. Writing qt.conf and diagnostic scripts (run_debug.cmd, etc.)
7. Copying GL/ANGLE fallback DLLs for graphics compatibility
8. Copying assets and creating a distributable package

Usage:
    python scripts/build-windows.py                    # Auto-installs missing deps, then builds
    python scripts/build-windows.py --no-auto-install  # Show manual install instructions instead
    python scripts/build-windows.py --skip-checks      # Skip dependency checks entirely
    python scripts/build-windows.py --build-type Debug # Build in Debug mode
    python scripts/build-windows.py --clean            # Clean build directory first
    python scripts/build-windows.py --deploy-only      # Only deploy Qt (assumes built)
    python scripts/build-windows.py --no-package       # Skip ZIP creation (faster for dev)
    python scripts/build-windows.py --help             # Show this help

Behavior:
    By default, the script will automatically install missing dependencies using winget.
    Use --no-auto-install to disable this and see manual installation instructions instead.
    
Performance Tips for VMs:
    set CMAKE_BUILD_PARALLEL_LEVEL=2    # Limit parallel jobs to avoid thrashing
    set PYTHONUNBUFFERED=1              # Show output immediately (no buffering)
    set NINJA_STATUS=[%%f/%%t] %%e sec  # Show Ninja build progress
    
    Use --no-package during development to skip slow ZIP creation

Requirements:
    - Python 3.7+
    - CMake 3.21+ (auto-installed if missing)
    - Ninja build system (auto-installed if missing)
    - Visual Studio 2019/2022 with C++ tools (auto-installed if missing)
    - Qt 6.6.3 or compatible (must be installed manually from qt.io)
    
Note:
    - Auto-install requires Windows 10/11 with winget
    - Qt cannot be auto-installed (winget Qt packages lack required components)
    - Run as Administrator if auto-install fails with permission errors
    - Long operations (compile, windeployqt, ZIP) show warnings but no progress bars
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
    print(f"{Color.GREEN}[+]{Color.RESET} {msg}")

def warning(msg: str) -> None:
    """Print warning message."""
    print(f"{Color.YELLOW}[!]{Color.RESET} {msg}")

def error(msg: str) -> None:
    """Print error message."""
    print(f"{Color.RED}[x]{Color.RESET} {msg}")

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
    
    
    if not os.environ.get('PYTHONUNBUFFERED'):
        warning("Tip: Set PYTHONUNBUFFERED=1 for immediate output visibility")
        warning("     Run: set PYTHONUNBUFFERED=1 (before running this script)")
        print()

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
    
    possible_paths = [
        Path(r"C:\Qt"),
        Path(r"C:\Program Files\Qt"),
        Path(r"C:\Program Files (x86)\Qt"),
        Path.home() / "Qt",
        Path(os.environ.get('QT_ROOT', '')) if os.environ.get('QT_ROOT') else None,
    ]

    
    qmake_in_path = shutil.which('qmake') or shutil.which('qmake.exe')
    if qmake_in_path:
        try:
            qmake_path = Path(qmake_in_path)
            
            arch_dir = qmake_path.parent.parent
            
            if 'msvc' in arch_dir.name.lower():
                return arch_dir
            
        except Exception:
            pass

    
    for base_path in possible_paths:
        if not base_path:
            continue
        if base_path.exists():
            
            for qt_version_dir in sorted(base_path.glob("6.*"), reverse=True):
                for arch_dir in qt_version_dir.glob("msvc*_64"):
                    qmake = arch_dir / "bin" / "qmake.exe"
                    if qmake.exists():
                        return arch_dir

    
    env_qt = os.environ.get('QT_HOME') or os.environ.get('QT_INSTALL_DIR')
    if env_qt:
        env_path = Path(env_qt)
        if env_path.exists():
            for qt_version_dir in sorted(env_path.glob("6.*"), reverse=True):
                for arch_dir in qt_version_dir.glob("msvc*_64"):
                    qmake = arch_dir / "bin" / "qmake.exe"
                    if qmake.exists():
                        return arch_dir

    return None


def find_qt_mingw() -> Optional[Path]:
    """Check if MinGW Qt is installed (to warn user)."""
    possible_paths = [
        Path(r"C:\Qt"),
        Path.home() / "Qt",
    ]
    
    for base_path in possible_paths:
        if base_path.exists():
            for qt_version_dir in sorted(base_path.glob("6.*"), reverse=True):
                for arch_dir in qt_version_dir.glob("mingw*"):
                    qmake = arch_dir / "bin" / "qmake.exe"
                    if qmake.exists():
                        return arch_dir
    return None

def check_qt() -> Tuple[bool, Optional[Path], Optional[str]]:
    """Check if Qt is installed."""
    qt_path = find_qt()
    
    if qt_path:
        qmake = qt_path / "bin" / "qmake.exe"
        if qmake.exists():
            try:
                result = run_command([str(qmake), '-query', 'QT_VERSION'], capture_output=True)
                version = result.stdout.strip()
                success(f"Qt {version} (MSVC) found at {qt_path}")
                return True, qt_path, version
            except (subprocess.CalledProcessError, FileNotFoundError):
                pass

    
    mingw_qt = find_qt_mingw()
    if mingw_qt:
        qmake = mingw_qt / "bin" / "qmake.exe"
        try:
            result = run_command([str(qmake), '-query', 'QT_VERSION'], capture_output=True)
            version = result.stdout.strip()
            error(f"Qt {version} (MinGW) found at {mingw_qt}")
            error("This project requires Qt with MSVC compiler, not MinGW")
            print()
            info(f"{Color.BOLD}Solution:{Color.RESET}")
            info("Run the Qt Maintenance Tool to add MSVC component:")
            print()
            maintenance_tool = mingw_qt.parent.parent / "MaintenanceTool.exe"
            if maintenance_tool.exists():
                info(f"1. Run: {maintenance_tool}")
            else:
                info(f"1. Find Qt Maintenance Tool in: {mingw_qt.parent.parent}")
            info("2. Click 'Add or remove components'")
            info(f"3. Expand 'Qt {version.split('.')[0]}.{version.split('.')[1]}'")
            info("4. CHECK: 'MSVC 2019 64-bit' or 'MSVC 2022 64-bit'")
            info("5. CHECK: 'Qt 5 Compatibility Module'")
            info("6. CHECK: 'Qt Multimedia'")
            info("7. Click 'Update' and wait for installation")
            info("8. Restart terminal and run this script again")
            print()
        except Exception:
            pass

    
    qmake_exec = shutil.which('qmake') or shutil.which('qmake.exe')
    if qmake_exec and not mingw_qt:
        try:
            result = run_command([qmake_exec, '-v'], capture_output=True)
            
            if 'mingw' in result.stdout.lower():
                error("Qt found but it's MinGW build - MSVC build required")
                return False, None, None
            
            
            try:
                out = run_command([qmake_exec, '-query', 'QT_VERSION'], capture_output=True)
                version = out.stdout.strip()
            except Exception:
                version = "unknown"
            
            success(f"Qt {version} found (qmake on PATH: {qmake_exec})")
            
            qmake_path = Path(qmake_exec)
            arch_dir = qmake_path.parent.parent
            return True, arch_dir, version
        except Exception:
            pass

    return False, None, None

def setup_msvc_environment() -> dict:
    """Set up MSVC environment variables."""
    info("Setting up MSVC environment...")
    
    vswhere = find_vswhere()
    if not vswhere:
        error("Cannot find vswhere.exe")
        sys.exit(1)
    
    
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
    
    
    vcvarsall = vs_path / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
    if not vcvarsall.exists():
        error(f"Cannot find vcvarsall.bat at {vcvarsall}")
        sys.exit(1)
    
    
    info(f"Running vcvarsall.bat from {vs_path}")
    cmd = f'"{vcvarsall}" x64 && set'
    
    result = subprocess.run(
        cmd,
        shell=True,
        capture_output=True,
        text=True
    )
    
    
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

def check_winget_available() -> bool:
    """Check if winget is available."""
    try:
        result = run_command(['winget', '--version'], capture_output=True, check=False)
        return result.returncode == 0
    except FileNotFoundError:
        return False

def install_visual_studio_direct() -> bool:
    """Download and install Visual Studio using the bootstrapper directly."""
    import tempfile
    import urllib.request
    
    info("Attempting direct Visual Studio installation...")
    info("This will download ~3MB bootstrapper, then download the full installer")
    print()
    
    
    vs_url = "https://aka.ms/vs/17/release/vs_community.exe"
    
    try:
        with tempfile.TemporaryDirectory() as tmpdir:
            installer_path = os.path.join(tmpdir, "vs_community.exe")
            
            info("Downloading Visual Studio installer...")
            urllib.request.urlretrieve(vs_url, installer_path)
            success("Installer downloaded")
            
            info("Running Visual Studio installer...")
            info("This may take 10-30 minutes depending on your internet connection")
            print()
            
            
            cmd = [
                installer_path,
                "--add", "Microsoft.VisualStudio.Workload.NativeDesktop",
                "--includeRecommended",
                "--passive",  
                "--norestart",
                "--wait"
            ]
            
            result = subprocess.run(cmd, check=False)
            
            if result.returncode == 0 or result.returncode == 3010:  
                success("Visual Studio installed successfully!")
                if result.returncode == 3010:
                    warning("A restart may be required to complete the installation")
                return True
            else:
                error(f"Visual Studio installation failed (exit code: {result.returncode})")
                return False
                
    except Exception as e:
        error(f"Failed to download/install Visual Studio: {e}")
        return False


def install_qt_interactive() -> bool:
    """Download and launch Qt online installer with instructions."""
    import tempfile
    import urllib.request
    
    print()
    info(f"{Color.BOLD}Qt Installation Helper{Color.RESET}")
    info("Qt requires an interactive installation (needs Qt account)")
    print()
    info("This will:")
    info("  1. Download the Qt online installer (~2MB)")
    info("  2. Launch it for you")
    info("  3. Guide you through the installation")
    print()
    
    
    qt_urls = [
        "https://d13lb3tujbc8s0.cloudfront.net/onlineinstallers/qt-unified-windows-x64-online.exe",
        "https://download.qt.io/archive/online_installers/4.8/qt-unified-windows-x64-4.8.0-online.exe",
        "https://download.qt.io/official_releases/online_installers/qt-unified-windows-x64-4.8-online.exe",
    ]
    qt_urls = [
        "https://d13lb3tujbc8s0.cloudfront.net/onlineinstallers/qt-unified-windows-x64-online.exe",
        "https://download.qt.io/archive/online_installers/4.8/qt-unified-windows-x64-4.8.0-online.exe",
        "https://download.qt.io/official_releases/online_installers/qt-unified-windows-x64-4.8-online.exe",
    ]
    
    try:
        with tempfile.TemporaryDirectory() as tmpdir:
            installer_path = os.path.join(tmpdir, "qt-installer.exe")
            
            
            downloaded = False
            for qt_url in qt_urls:
                try:
                    info(f"Downloading Qt online installer from {qt_url.split('/')[2]}...")
                    urllib.request.urlretrieve(qt_url, installer_path)
                    success("Qt installer downloaded")
                    downloaded = True
                    break
                except Exception as e:
                    warning(f"Failed to download from {qt_url.split('/')[2]}: {e}")
                    continue
            
            if not downloaded:
                error("Could not download Qt installer from any mirror")
                print()
                info("Please download manually from:")
                info("https://www.qt.io/download-qt-installer")
                print()
                info("Then run the installer and follow the steps below:")
                print()
                print(f"{Color.BOLD}=== Installation Steps ==={Color.RESET}")
                print()
                print(f"{Color.BOLD}1. Qt Account:{Color.RESET}")
                print("   - Login with your Qt account (or create one - it's free)")
                print()
                print(f"{Color.BOLD}2. Installation Path:{Color.RESET}")
                print("   - Use default: C:\\Qt")
                print("   - Or custom path and set QT_ROOT environment variable")
                print()
                print(f"{Color.BOLD}3. Select Components (CRITICAL):{Color.RESET}")
                print("   - Expand 'Qt 6.6.3' (or latest 6.x)")
                print("   - CHECK: 'MSVC 2019 64-bit' (or 'MSVC 2022 64-bit')")
                print("   - CHECK: 'Qt 5 Compatibility Module'")
                print("   - CHECK: 'Qt Multimedia'")
                print("   - Uncheck everything else to save space")
                print()
                print(f"{Color.BOLD}4. After Installation:{Color.RESET}")
                print("   - Restart this terminal")
                print("   - Run this script again")
                print()
                
                
                try:
                    import webbrowser
                    webbrowser.open("https://www.qt.io/download-qt-installer")
                    success("Opened Qt download page in your browser")
                except Exception:
                    pass
                
                return False
            
            print()
            
            print(f"{Color.BOLD}=== IMPORTANT: Follow these steps in the installer ==={Color.RESET}")
            print()
            print(f"{Color.BOLD}1. Qt Account:{Color.RESET}")
            print("   - Login with your Qt account (or create one - it's free)")
            print()
            print(f"{Color.BOLD}2. Installation Path:{Color.RESET}")
            print("   - Use default: C:\\Qt")
            print("   - Or custom path and set QT_ROOT environment variable")
            print()
            print(f"{Color.BOLD}3. Select Components (CRITICAL):{Color.RESET}")
            print("   - Expand 'Qt 6.6.3' (or latest 6.x)")
            print("   - CHECK: 'MSVC 2019 64-bit' (or 'MSVC 2022 64-bit')")
            print("   - CHECK: 'Qt 5 Compatibility Module'")
            print("   - CHECK: 'Qt Multimedia'")
            print("   - Uncheck everything else to save space")
            print()
            print(f"{Color.BOLD}4. After Installation:{Color.RESET}")
            print("   - Restart this terminal")
            print("   - Run this script again")
            print()
            print(f"{Color.BOLD}Press Enter to launch the Qt installer...{Color.RESET}")
            input()
            
            info("Launching Qt installer...")
            
            subprocess.Popen([installer_path], shell=True)
            
            print()
            success("Qt installer launched!")
            warning("Complete the installation, then restart your terminal and run this script again")
            print()
            return False  
                
    except Exception as e:
        error(f"Failed to download/launch Qt installer: {e}")
        return False


def auto_install_dependencies() -> bool:
    """Attempt to auto-install missing dependencies using winget."""
    if not check_winget_available():
        warning("winget not available - cannot auto-install")
        warning("Please install dependencies manually or update to Windows 10/11 with winget")
        return False
    
    info("Attempting to auto-install missing dependencies using winget...")
    print()
    
    
    cmake_ok, _ = check_cmake()
    ninja_ok = check_ninja()
    msvc_ok, _ = check_msvc()
    qt_ok, _, _ = check_qt()
    
    installs_needed = []
    if not cmake_ok:
        installs_needed.append(('CMake', 'Kitware.CMake'))
    if not ninja_ok:
        installs_needed.append(('Ninja', 'Ninja-build.Ninja'))
    if not msvc_ok:
        installs_needed.append(('Visual Studio 2022', 'Microsoft.VisualStudio.2022.Community', 
                               '--override "--add Microsoft.VisualStudio.Workload.NativeDesktop"'))
    
    if not installs_needed and not qt_ok:
        warning("Qt installation requires manual download from qt.io")
        warning("winget Qt packages may not include required MSVC toolchain")
        print()
        info("Would you like to download and run the Qt installer now?")
        
        
        if install_qt_interactive():
            return True
        else:
            
            return False
    
    if not installs_needed:
        return True
    
    print(f"{Color.BOLD}Installing the following packages:{Color.RESET}")
    for item in installs_needed:
        print(f"  - {item[0]}")
    print()
    
    success_count = 0
    needs_restart = False
    vs_failed = False
    vs_needed = False
    
    for item in installs_needed:
        name = item[0]
        package_id = item[1]
        extra_args = item[2] if len(item) > 2 else None
        
        info(f"Installing {name}...")
        print(f"  This may take several minutes, please wait...")
        print()
        
        cmd = ['winget', 'install', '--id', package_id, '--accept-package-agreements', '--accept-source-agreements']
        if extra_args:
            cmd.extend(extra_args.split())
        
        
        try:
            result = subprocess.run(cmd, check=False)
            print()  
            
            if result.returncode == 0:
                success(f"{name} installed successfully")
                success_count += 1
                
                
                if 'CMake' in name or 'Ninja' in name:
                    needs_restart = True
            else:
                error(f"Failed to install {name} (exit code: {result.returncode})")
                
                
                if 'Visual Studio' in name:
                    vs_failed = True
                    vs_needed = True
                else:
                    warning("You may need to run this script as Administrator")
        except Exception as e:
            error(f"Error installing {name}: {e}")
        
        print()  
    
    print()
    
    
    if vs_failed:
        print()
        warning("winget failed to install Visual Studio (common issue)")
        info("Attempting direct installation using VS bootstrapper...")
        print()
        
        if install_visual_studio_direct():
            success("Visual Studio installed via direct download!")
            success_count += 1
            vs_failed = False
        else:
            print()
            info(f"{Color.BOLD}Visual Studio Installation Failed{Color.RESET}")
            info("Please install manually:")
            info("1. Download: https://visualstudio.microsoft.com/downloads/")
            info("2. Run the installer")
            info("3. Select 'Desktop development with C++'")
            info("4. Install and restart this script")
            print()
    
    
    print()
    if success_count == len(installs_needed):
        success("All dependencies installed successfully!")
        if needs_restart:
            warning("CMake/Ninja were installed - you must restart this terminal!")
            warning("Close this terminal, open a new one, and run this script again.")
        return True
    elif success_count > 0:
        warning(f"Installed {success_count}/{len(installs_needed)} dependencies")
        
        if needs_restart:
            print()
            info(f"{Color.BOLD}IMPORTANT: CMake/Ninja were just installed!{Color.RESET}")
            info("You MUST restart your terminal for PATH changes to take effect.")
            info("Then run this script again to continue.")
            print()
        
        warning("Please install remaining dependencies manually")
        print_installation_guide()
        return False
    else:
        error("Auto-install failed")
        warning("Please install dependencies manually")
        print_installation_guide()
        return False

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
        return False, None
    
    success("All dependencies found!")
    return True, qt_path

def configure_project(build_dir: Path, build_type: str, qt_path: Optional[Path], 
                     msvc_env: dict) -> None:
    """Configure the project with CMake."""
    info(f"Configuring project (Build type: {build_type})...")
    print("  Running CMake configuration...")
    
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
    
    
    parallel = os.environ.get('CMAKE_BUILD_PARALLEL_LEVEL')
    if parallel:
        info(f"  Build parallelism limited to {parallel} jobs")
    
    run_command(cmake_args, env=msvc_env)
    success("Project configured")

def build_project(build_dir: Path, msvc_env: dict) -> None:
    """Build the project."""
    info("Building project...")
    
    parallel = os.environ.get('CMAKE_BUILD_PARALLEL_LEVEL')
    if parallel:
        info(f"  Using {parallel} parallel jobs (set via CMAKE_BUILD_PARALLEL_LEVEL)")
    else:
        warning("  Using all CPU cores - set CMAKE_BUILD_PARALLEL_LEVEL=2 to reduce load in VMs")
    
    print("  Compiling (this may take several minutes)...")
    
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
    
    
    mode_flag = {
        "Debug": "--debug",
        "Release": "--release",
        "RelWithDebInfo": "--release",  
    }[build_type]
    
    run_command([
        str(windeployqt),
        mode_flag,
        '--compiler-runtime',  
        '--qmldir', str(qml_dir),
        str(exe_path)
    ])
    
    success("Qt dependencies deployed")

def write_qt_conf(app_dir: Path) -> None:
    """Write qt.conf to configure Qt plugin paths."""
    info("Writing qt.conf...")
    
    qt_conf_content = """[Paths]
Plugins = .
Imports = qml
Qml2Imports = qml
Translations = translations
"""
    
    qt_conf_path = app_dir / "qt.conf"
    qt_conf_path.write_text(qt_conf_content, encoding='ascii')
    success("qt.conf written")

def write_debug_scripts(app_dir: Path, app_name: str) -> None:
    """Write diagnostic scripts for troubleshooting."""
    info("Writing diagnostic scripts...")
    
    
    run_smart_content = """@echo off
setlocal
cd /d "%~dp0"

echo ============================================
echo Standard of Iron - Smart Launcher
echo ============================================
echo.

REM Try OpenGL first
echo [1/2] Attempting OpenGL rendering...
"%~dp0{app_name}.exe" 2>&1
set EXIT_CODE=%ERRORLEVEL%

REM Check for crash (access violation = -1073741819)
if %EXIT_CODE% EQU -1073741819 (
    echo.
    echo [CRASH] OpenGL rendering failed!
    echo [INFO] This is common on VMs or older hardware
    echo.
    echo [2/2] Retrying with software rendering...
    echo.
    set QT_QUICK_BACKEND=software
    set QT_OPENGL=software
    "%~dp0{app_name}.exe"
    exit /b %ERRORLEVEL%
)

if %EXIT_CODE% NEQ 0 (
    echo.
    echo [ERROR] Application exited with code: %EXIT_CODE%
    echo [HINT] Try running run_debug.cmd for detailed logs
    pause
    exit /b %EXIT_CODE%
)

exit /b 0
""".format(app_name=app_name)
    
    run_smart_path = app_dir / "run.cmd"
    run_smart_path.write_text(run_smart_content, encoding='ascii')
    
    
    run_debug_content = """@echo off
setlocal
cd /d "%~dp0"
set QT_DEBUG_PLUGINS=1
set QT_LOGGING_RULES=qt.*=true;qt.qml=true;qqml.*=true;qt.rhi.*=true
set QT_OPENGL=desktop
set QT_QPA_PLATFORM=windows
echo Starting with Desktop OpenGL...
echo Logging to runlog.txt
"%~dp0{app_name}.exe" 1> "%~dp0runlog.txt" 2>&1
echo ExitCode: %ERRORLEVEL%>> "%~dp0runlog.txt"
type "%~dp0runlog.txt"
pause
""".format(app_name=app_name)
    
    run_debug_path = app_dir / "run_debug.cmd"
    run_debug_path.write_text(run_debug_content, encoding='ascii')
    
    
    run_debug_softwaregl_content = """@echo off
setlocal
cd /d "%~dp0"
set QT_DEBUG_PLUGINS=1
set QT_LOGGING_RULES=qt.*=true;qt.qml=true;qqml.*=true;qt.quick.*=true;qt.rhi.*=true
set QT_OPENGL=software
set QT_QUICK_BACKEND=software
set QT_QPA_PLATFORM=windows
set QMLSCENE_DEVICE=softwarecontext
echo Starting with Qt Quick Software Renderer (no OpenGL)...
echo This is the safest mode for VMs and old hardware
echo Logging to runlog_software.txt
"%~dp0{app_name}.exe" 1> "%~dp0runlog_software.txt" 2>&1
echo ExitCode: %ERRORLEVEL%>> "%~dp0runlog_software.txt"
type "%~dp0runlog_software.txt"
pause
""".format(app_name=app_name)
    
    run_debug_softwaregl_path = app_dir / "run_debug_softwaregl.cmd"
    run_debug_softwaregl_path.write_text(run_debug_softwaregl_content, encoding='ascii')
    
    
    run_debug_angle_content = """@echo off
setlocal
cd /d "%~dp0"
set QT_DEBUG_PLUGINS=1
set QT_LOGGING_RULES=qt.*=true;qt.qml=true;qqml.*=true;qt.rhi.*=true
set QT_OPENGL=angle
set QT_ANGLE_PLATFORM=d3d11
set QT_QPA_PLATFORM=windows
echo Starting with ANGLE (OpenGL ES via D3D11)...
echo Logging to runlog_angle.txt
"%~dp0{app_name}.exe" 1> "%~dp0runlog_angle.txt" 2>&1
echo ExitCode: %ERRORLEVEL%>> "%~dp0runlog_angle.txt"
type "%~dp0runlog_angle.txt"
pause
""".format(app_name=app_name)
    
    run_debug_angle_path = app_dir / "run_debug_angle.cmd"
    run_debug_angle_path.write_text(run_debug_angle_content, encoding='ascii')

    success(f"Diagnostic scripts written: run.cmd (smart), run_debug.cmd, run_debug_softwaregl.cmd, run_debug_angle.cmd")


def copy_gl_angle_fallbacks(app_dir: Path, qt_path: Path) -> None:
    """Copy GL/ANGLE fallback DLLs for graphics compatibility."""
    info("Copying GL/ANGLE fallback DLLs...")
    
    qt_bin = qt_path / "bin"
    fallback_dlls = [
        "d3dcompiler_47.dll",
        "opengl32sw.dll",
        "libEGL.dll",
        "libGLESv2.dll"
    ]
    
    copied_count = 0
    for dll_name in fallback_dlls:
        src = qt_bin / dll_name
        if src.exists():
            dst = app_dir / dll_name
            shutil.copy2(src, dst)
            info(f"  Copied {dll_name}")
            copied_count += 1
        else:
            warning(f"  {dll_name} not found in Qt bin directory")
    
    if copied_count > 0:
        success(f"Copied {copied_count} GL/ANGLE fallback DLL(s)")
    else:
        warning("No GL/ANGLE fallback DLLs found")

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
    warning("This may take several minutes with no progress indicator...")
    print("  Compressing files (CPU-intensive, please wait)...")
    
    app_dir = build_dir / "bin"
    package_name = f"standard_of_iron-win-x64-{build_type}.zip"
    package_path = Path(package_name)
    
    if package_path.exists():
        package_path.unlink()
    
    import time
    start_time = time.time()
    
    shutil.make_archive(
        package_path.stem,
        'zip',
        app_dir
    )
    
    elapsed = time.time() - start_time
    success(f"Package created: {package_path} (took {elapsed:.1f}s)")
    return package_path

def main() -> int:
    """Main entry point."""
    import time
    start_time = time.time()
    
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
        '--no-auto-install',
        action='store_true',
        help='Do NOT auto-install missing dependencies (show manual instructions instead)'
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
    
    
    check_windows()
    
    
    repo_root = Path(__file__).parent.parent
    os.chdir(repo_root)
    
    build_dir = Path("build")
    qt_path = None
    
    
    if not args.skip_checks and not args.deploy_only:
        deps_ok, qt_path = check_dependencies()
        
        if not deps_ok:
            
            if not args.no_auto_install:
                info("Attempting auto-install of missing dependencies...")
                print()
                if auto_install_dependencies():
                    warning("Dependencies installed - please restart your terminal and run this script again")
                    return 1
                else:
                    print()
                    print_installation_guide()
                    return 1
            else:
                info("Auto-install disabled (--no-auto-install flag)")
                print()
                print_installation_guide()
                return 1
    else:
        
        _, qt_path, _ = check_qt()
    
    
    if args.clean and build_dir.exists():
        info("Cleaning build directory...")
        shutil.rmtree(build_dir)
        success("Build directory cleaned")
    
    
    if not args.deploy_only:
        msvc_env = setup_msvc_environment()
        
        
        configure_project(build_dir, args.build_type, qt_path, msvc_env)
        
        
        build_project(build_dir, msvc_env)
    
    
    if qt_path:
        deploy_qt(build_dir, qt_path, "standard_of_iron", args.build_type)
        
        
        app_dir = build_dir / "bin"
        write_qt_conf(app_dir)
        
        
        write_debug_scripts(app_dir, "standard_of_iron")
        
        
        copy_gl_angle_fallbacks(app_dir, qt_path)
    else:
        warning("Qt path not found, skipping windeployqt")
    
    
    copy_assets(build_dir)
    
    
    if not args.no_package:
        package_path = create_package(build_dir, args.build_type)
        print()
        elapsed = time.time() - start_time
        success(f"Build complete! (total time: {elapsed:.1f}s)")
        info(f"Package: {package_path}")
        info(f"Executable: {build_dir / 'bin' / 'standard_of_iron.exe'}")
    else:
        print()
        elapsed = time.time() - start_time
        success(f"Build complete! (total time: {elapsed:.1f}s)")
        info(f"Executable: {build_dir / 'bin' / 'standard_of_iron.exe'}")
        info("Run with debug scripts: run_debug.cmd, run_debug_softwaregl.cmd, run_debug_angle.cmd")
    
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
