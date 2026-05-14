
# Contributing to Standard of Iron

Thank you for your interest in contributing to Standard of Iron! This document provides guidelines and information to help you contribute effectively.

## Development Setup

### Prerequisites

To build and develop Standard of Iron, you'll need:

- **CMake** >= 3.21.0
- **GCC/G++** >= 10.0.0 or equivalent C++20 compiler
- **Qt5** or **Qt6** (Qt6 is preferred)
  - Qt Core, Widgets, OpenGL, Quick, Qml, QuickControls2
- **OpenGL** 3.3+ support

### Installation

Run the automated setup:

```bash
make install
````

This will install all required dependencies on Ubuntu/Debian-based systems.

## Code Formatting

We maintain consistent code style across the entire codebase using automated formatting tools.

### Required Tools

1. **clang-format** (required)

   * Formats C/C++ source files (`.cpp`, `.h`, `.hpp`)
   * Formats GLSL shader files (`.frag`, `.vert`)
   * Installed automatically with `make install`

2. **qmlformat** (optional but recommended)

   * Formats QML files (`.qml`)
   * Part of Qt development tools
   * Installed with `qtdeclarative5-dev-tools` (Qt5) or `qt6-declarative-dev-tools` (Qt6)

### Formatting Commands

Format all code before committing:

```bash
make format
```

This will:

1. Apply clang-tidy fixes to changed C/C++ files
2. Format C/C++ files with clang-format
3. Format QML files with qmlformat (if available)
4. Format shader files with clang-format
5. Format Python files with black (if available)

Comment stripping is intentionally separate because it is destructive:

```bash
make format-strip-comments
```

Check if code is properly formatted (CI-friendly):

```bash
make format-check
```

CI uses a faster changed-file variant to keep pull request builds responsive:

```bash
make format-check-ci
```

### File Types Covered

* **C/C++ files**: `.cpp`, `.c`, `.h`, `.hpp`
* **QML files**: `.qml`
* **Shader files**: `.frag`, `.vert`

### Installing qmlformat

#### Ubuntu/Debian (Qt5)

```bash
sudo apt-get install qtdeclarative5-dev-tools
```

#### Ubuntu/Debian (Qt6)

```bash
sudo apt-get install qt6-declarative-dev-tools
```

The `qmlformat` binary will typically be installed to:

* Qt5: `/usr/lib/qt5/bin/qmlformat`
* Qt6: `/usr/lib/qt6/bin/qmlformat`

The Makefile automatically detects qmlformat in these locations.

## Building the Project

### Standard Build

```bash
make build
```

### Debug Build

```bash
make debug
```

### Release Build

```bash
make release
```

### Clean Build

```bash
make rebuild
```

## Running the Application

### Run the Game

```bash
make run
```

### Run the Map Editor

```bash
make editor
```

### Run the Arena Playground

```bash
make arena
```

### Run in Headless Mode (for CI)

```bash
make run-headless
```

## Testing

Run tests (when implemented):

```bash
make test
```

## Code Style Guidelines

### C++ Style

* Follow the `.clang-format` configuration
* Use C++20 features appropriately
* 4-space indentation (no tabs)
* 88 character line limit
* Place braces on the same line (`Attach` style)

### QML Style

* Use qmlformat's default style
* Keep property ordering consistent
* Use proper indentation for nested elements

### Shader Style

* Use clang-format for consistent indentation
* Follow GLSL naming conventions
* Comment complex shader operations when necessary

## Commit Guidelines

1. **Format your code**
   Always run:

   ```bash
   make format
   ```

2. **Ensure the project builds successfully**
   Run:

   ```bash
   make build
   ```

   This also regenerates ignored creature assets in `assets/creatures/`.

3. **Test your changes**
   Run relevant tests and verify the game launches correctly.

4. **Write clear commit messages**
   Describe what changed and why.

## Pull Request Process

1. Fork the repository

2. Create a feature branch:

   ```bash
   git checkout -b feature/amazing-feature
   ```

3. Make your changes

4. Format your code:

   ```bash
   make format
   ```

5. Ensure the project builds successfully:

   ```bash
   make build
   ```

6. Commit your changes with a clear commit message

7. Push your branch:

   ```bash
   git push origin feature/amazing-feature
   ```

8. Open a Pull Request

## Contributor License Agreement

By submitting a contribution (including code, assets, documentation, shaders, maps, audio, or other content) to Standard of Iron, you agree that:

* Your contribution is your original work, or you have the legal right to submit it
* You grant the project and its maintainers the right to use, modify, distribute, sublicense, and commercially distribute your contribution under the project's license
* Your contribution will be licensed under the same license as the project unless explicitly stated otherwise

This includes distribution through commercial platforms such as Steam or other marketplaces.

## Third-Party Content

Do not submit assets, code, music, fonts, textures, models, or other content unless:

* You created them yourself, or
* They are compatible with the project's license and permit commercial redistribution

If you use third-party resources, clearly document their source and license in your Pull Request.

## Code of Conduct

Please be respectful and constructive in discussions, reviews, and contributions.

Harassment, abusive behavior, or intentionally disruptive conduct will not be tolerated.

## Questions or Issues?

If you have questions or encounter issues:

* Open an issue on GitHub
* Check existing issues and discussions
* Review the README.md for additional information

## License

Standard of Iron is licensed under the MIT License.

By contributing to this project, you agree that your contributions will be licensed under the MIT License unless explicitly stated otherwise.
