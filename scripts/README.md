# Scripts Directory

This directory contains utility scripts for building and maintaining the Standard-of-Iron project.

## Build Scripts

### `build-windows.py` - Windows Local Build Script

A user-friendly Python script that automates the Windows build process. It verifies dependencies, sets up the MSVC environment, builds the project, and creates a distributable package.

**Features:**
- ✓ Checks for required dependencies (CMake, Ninja, MSVC, Qt)
- ✓ Provides installation guidance for missing tools
- ✓ Automatically sets up MSVC environment
- ✓ Builds the project with proper configuration
- ✓ Deploys Qt dependencies with windeployqt
- ✓ Copies game assets
- ✓ Creates distributable ZIP package

**Requirements:**
- Python 3.7+
- CMake 3.21+
- Ninja build system
- Visual Studio 2019/2022 with C++ tools
- Qt 6.6.3 or compatible (with msvc2019_64 or msvc2022_64)

**Usage:**
```bash
# Full build with dependency checks
python scripts/build-windows.py

# Build in Debug mode
python scripts/build-windows.py --build-type Debug

# Clean build (remove build directory first)
python scripts/build-windows.py --clean

# Skip dependency checks (faster if you know everything is installed)
python scripts/build-windows.py --skip-checks

# Only deploy Qt dependencies (assumes project is already built)
python scripts/build-windows.py --deploy-only

# Build without creating package
python scripts/build-windows.py --no-package

# Show help
python scripts/build-windows.py --help
```

**Output:**
- Built executable: `build/bin/standard_of_iron.exe`
- Distributable package: `standard_of_iron-win-x64-Release.zip`

**Troubleshooting:**

If the script can't find Qt, set the `QT_ROOT` environment variable:
```powershell
$env:QT_ROOT = "C:\Qt\6.6.3\msvc2019_64"
python scripts/build-windows.py
```

If you encounter MSVC issues, ensure "Desktop development with C++" is installed in Visual Studio.

---

## Contributor Tracking

### `update_contributors.py` - Contributor Tracking Script

Automatically generates and maintains a `CONTRIBUTORS.md` file based on Git commit history. This script uses Git as the single source of truth for contributor information.

**Features:**
- ✓ Extracts contributor names, emails, and commit dates from Git history
- ✓ Counts total commits per contributor
- ✓ Tracks first and last contribution dates
- ✓ Generates formatted Markdown table
- ✓ Includes optional reference link column (manual editing supported)
- ✓ No external dependencies beyond Git and Python 3

**Requirements:**
- Python 3.7+
- Git

**Usage:**
```bash
# Update contributor list
python scripts/update_contributors.py
```

**Output:**
- File: `CONTRIBUTORS.md` (repository root)
- Contains a Markdown table with contributor information

**Example Output:**
```markdown
# 🌍 Project Contributors

| Name | Email | Contributions | First Commit | Last Commit | Reference |
|------|--------|----------------|---------------|--------------|-----------|
| Alice Dev | alice@example.com | 42 commits | 2024-03-11 | 2025-10-28 |  |
| Bob Writer | bob@studio.com | 9 commits | 2025-03-15 | 2025-10-20 |  |
```

**Note:** The Reference column is left empty by default and can be manually edited to add links to GitHub profiles, websites, or other references. The script automatically preserves any manually-added Reference values when it regenerates the file, so your edits will not be lost on subsequent runs.

---

## Linux/macOS Scripts

### `setup-deps.sh` - Dependency Installer

Verifies and installs required toolchain and Qt/QML runtime modules for Linux (Debian/Ubuntu, Arch/Manjaro) and macOS.

**Usage:**
```bash
./scripts/setup-deps.sh                  # Interactive install
./scripts/setup-deps.sh --yes            # Non-interactive (assume yes)
./scripts/setup-deps.sh --dry-run        # Show actions without installing
./scripts/setup-deps.sh --no-install     # Only verify, don't install
```

---

## Translation Scripts

### `csv2ts.sh` - CSV to Qt Translation Files

Converts CSV translation files to Qt `.ts` format.

### `ts2csv.sh` - Qt Translation Files to CSV

Exports Qt `.ts` translation files to CSV format for easier editing.

**Usage:**
```bash
./scripts/csv2ts.sh    # Convert CSV to .ts files
./scripts/ts2csv.sh    # Convert .ts files to CSV
```

---

## Development Scripts

### `validate_shader_uniforms.py` - Shader Uniform Validation

**⚠️ IMPORTANT**: Always run this after modifying shader code or backend uniform handling!

Validates that uniform names in GLSL shaders match the names used in C++ backend code. 
Prevents rendering bugs caused by naming convention mismatches (camelCase vs snake_case).

**Features:**
- ✓ Scans all `.frag` and `.vert` shader files in `assets/shaders/`
- ✓ Extracts uniform declarations from shaders
- ✓ Compares with `uniformHandle()` calls in `render/gl/backend.cpp`
- ✓ Detects camelCase/snake_case mismatches
- ✓ Reports exact line numbers of issues
- ✓ Exit code 0 if valid, 1 if errors found (CI-friendly)

**Usage:**
```bash
# Validate all shaders and backend code
python3 scripts/validate_shader_uniforms.py

# Alternative bash version
./scripts/validate-shader-uniforms.sh
```

**Example Output:**
```
=== Shader Uniform Validation ===
Found 35 shader files
Total unique uniforms in shaders: 56
Found 50 unique uniformHandle() calls in backend.cpp

✓ All uniform names match between shaders and backend!
```

**When to Run:**
- After renaming variables in backend.cpp
- After modifying shader files
- Before committing rendering changes
- In CI/CD pipelines
- When debugging rendering issues

**Common Issues Found:**
- `u_view_proj` (backend) vs `u_viewProj` (shader) ❌
- `u_light_dir` (backend) vs `u_lightDir` (shader) ❌
- `u_tile_size` (backend) vs `u_tileSize` (shader) ❌

### `validate-shader-uniforms.sh` - Shader Validation (Bash)

Alternative bash implementation of shader uniform validation. Use the Python version for more detailed output.

### `run-clang-tidy-fixes.sh` - Clang-Tidy Auto-Fix

Runs clang-tidy with automatic fixes enabled.

### `debug-audio.sh` - Audio Debugging

Helper script for debugging audio issues.

### `remove-comments.sh` - Code Cleanup

Removes comments from source files (use with caution).

**Usage:**
```bash
./scripts/remove-comments.sh
```

---

## Platform-Specific Build Instructions

### Windows
1. Run `python scripts/build-windows.py`
2. Follow any installation prompts for missing dependencies
3. Find the executable at `build/bin/standard_of_iron.exe`
4. Distributable package: `standard_of_iron-win-x64-Release.zip`

### Linux/macOS
1. Run `./scripts/setup-deps.sh` to install dependencies
2. Build with CMake:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```
3. Find the executable at `build/bin/standard_of_iron`

---

## Contributing

When adding new scripts:
1. Add appropriate shebang line (`#!/usr/bin/env bash` or `#!/usr/bin/env python3`)
2. Make scripts executable: `chmod +x scripts/your-script.sh`
3. Include usage documentation in the script header
4. Update this README with script description and usage
5. Follow existing code style and patterns
