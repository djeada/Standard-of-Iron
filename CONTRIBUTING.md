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
```

This will install all required dependencies on Ubuntu/Debian-based systems.

## Code Formatting

We maintain consistent code style across the entire codebase using automated formatting tools.

### Required Tools

1. **clang-format** (required)
   - Formats C/C++ source files (`.cpp`, `.h`, `.hpp`)
   - Formats GLSL shader files (`.frag`, `.vert`)
   - Installed automatically with `make install`

2. **qmlformat** (optional but recommended)
   - Formats QML files (`.qml`)
   - Part of Qt development tools
   - Installed with `qtdeclarative5-dev-tools` (Qt5) or `qt6-declarative-dev-tools` (Qt6)

### Formatting Commands

Format all code before committing:
```bash
make format
```

This will:
1. Strip comments from C/C++ files
2. Format C/C++ files with clang-format
3. Format QML files with qmlformat (if available)
4. Format shader files with clang-format

Check if code is properly formatted (CI-friendly):
```bash
make format-check
```

### File Types Covered

- **C/C++ files**: `.cpp`, `.c`, `.h`, `.hpp`
- **QML files**: `.qml`
- **Shader files**: `.frag`, `.vert`

### Installing qmlformat

#### Ubuntu/Debian (Qt5)
```bash
sudo apt-get install qtdeclarative5-dev-tools
```

#### Ubuntu/Debian (Qt6)
```bash
sudo apt-get install qt6-declarative-dev-tools
```

The `qmlformat` binary will be installed to:
- Qt5: `/usr/lib/qt5/bin/qmlformat`
- Qt6: `/usr/lib/qt6/bin/qmlformat`

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
- Follow the `.clang-format` configuration
- Use C++20 features appropriately
- 4-space indentation (no tabs)
- 88 character line limit
- Place braces on the same line (`Attach` style)

### QML Style
- Use qmlformat's default style
- Consistent property ordering
- Proper indentation for nested elements

### Shader Style
- Use clang-format for consistent indentation
- Follow GLSL naming conventions
- Comment complex shader operations

## Commit Guidelines

1. **Format your code**: Always run `make format` before committing
2. **Build successfully**: Ensure `make build` completes without errors
3. **Test your changes**: Run relevant tests
4. **Write clear commit messages**: Describe what and why, not just how

## Pull Request Process

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Format your code (`make format`)
5. Commit your changes (`git commit -m 'Add amazing feature'`)
6. Push to the branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

## Questions or Issues?

If you have questions or encounter issues:
- Open an issue on GitHub
- Check existing issues and discussions
- Review the README.md for additional information

## License

By contributing to Standard of Iron, you agree that your contributions will be licensed under the same license as the project.
