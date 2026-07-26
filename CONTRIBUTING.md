
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

## Code Formatting, Linting and Quality

Formatting, linting and destructive source rewrites are three separate
operations with three separate commands. `make format` only ever changes
whitespace - it never deletes comments and never applies semantic refactors.

Every command below is a thin wrapper around one driver, `scripts/format.py`,
which is also what the pre-commit hooks and CI run. Tool versions are pinned in
`tools/versions.env`.

### Setting up the toolchain

```bash
make format-bootstrap   # install the pinned Python/Node tools, print system hints
make format-doctor      # report installed vs pinned versions
make hooks-install      # install the pre-commit git hooks
```

`format-doctor` fails when a required tool is missing or its *major* version
differs from the pin; a minor/patch difference is reported as drift.

### Everyday commands

| Command | What it does |
| --- | --- |
| `make format` | Format every tracked file in place |
| `make format-check` | Verify formatting, change nothing (the CI gate) |
| `make format-changed` | Format only files changed against `FORMAT_BASE` |
| `make format-check-changed` | Fast changed-files check |
| `make lint` | clang-tidy, qmllint, Ruff, ShellCheck, yamllint, JSON syntax |
| `make lint-fix` | Apply the linters' automated fixes - explicit and separate |
| `make lint-changed` | Lint only what changed (includes changed-file clang-tidy) |
| `make quality` | `format-check` + `lint` + quality-marker scan |
| `make validate` | `quality` + build + tests + content validation |
| `make strip-comments` | **Destructive**: delete comments. Needs `STRIP_COMMENTS_CONFIRM=1` |

`FORMAT_BASE` defaults to `origin/main`:

```bash
FORMAT_BASE=origin/develop make format-check-changed
```

The driver can also be called directly:

```bash
python scripts/format.py --all --fix
python scripts/format.py --all --check
python scripts/format.py --changed origin/main --check
python scripts/format.py --staged --fix
python scripts/format.py --files game/foo.cpp ui/qml/Hud.qml
python scripts/format.py --all --lint --deep      # includes whole-tree clang-tidy
```

### Coverage

| Language | Formatter | Linter |
| --- | --- | --- |
| C/C++ | clang-format | clang-tidy (changed files on PRs, full tree nightly) |
| GLSL (`.frag`, `.vert`, `.glsl`) | clang-format | - |
| QML | qmlformat | qmllint (advisory) |
| Python | black | Ruff |
| Shell | shfmt (optional) | ShellCheck |
| CMake | gersemi | - |
| YAML | prettier (optional) | yamllint |
| Markdown | prettier (optional) | markdownlint (advisory, optional) |
| JSON | prettier (optional, excludes `assets/`) | built-in syntax check |

Advisory linters report findings without failing the build; the nightly
workflow runs them with `--fail-on-advisory`. Generated game data under
`assets/` is never reformatted, only syntax-checked.

### Installing qmlformat and qmllint

```bash
sudo apt-get install qt6-declarative-dev-tools   # Ubuntu/Debian (Qt6)
sudo apt-get install qtdeclarative5-dev-tools    # Ubuntu/Debian (Qt5)
```

They land in `/usr/lib/qt6/bin/` or `/usr/lib/qt5/bin/`; the driver searches
those paths automatically, and `QMLFORMAT` / `QMLLINT` override the lookup.

## Continuous Integration

CI runs in three tiers of increasing thoroughness. Pull requests stay cheap so
review is not blocked for an hour; release tags are exhaustive, because that is
the point at which every supported platform must actually ship.

**Pull requests** (`.github/workflows/pr.yml`) run two jobs in parallel:

- `quality` - formatting, linting, quality markers and the static content
  validators. No compiler, fails in about a minute.
- `test` - Linux configure and build of the test targets only
  (`standard_of_iron_tests`, `content_validator`), then unit tests, content
  validation, the validator integration tests, and advisory clang-tidy over
  the changed files. The game binary, map editor, arena and preview tools are
  not built here.

**Nightly** (`.github/workflows/nightly.yml`) runs the whole-project
clang-tidy/qmllint pass, AddressSanitizer and UndefinedBehaviorSanitizer
lanes, a coverage report, and a full build, renderer self-test and packaging
dry run on Linux, macOS and Windows. This is what catches platform-specific
breakage between releases.

**Releases** (`.github/workflows/release.yml`) are fully gated:

```
preflight -> build (linux | macos | windows) -> verify -> publish
```

`preflight` is the same quality gate pull requests run, so no packaging time
is spent on a tag that cannot pass it. Each platform build compiles
everything, runs the tests, runs the packaged renderer self-test, produces its
installer and records a SHA-256 checksum. `verify` then asserts that every
supported OS is represented by exactly one package, that each package has a
checksum, and that every checksum matches. Only then does `publish` create the
release - and it re-reads the published release afterwards to confirm all
three downloads are attached. A single broken platform fails the whole
release rather than leaving a tag with a missing download.

`workflow_dispatch` on the release workflow builds and verifies release
candidates without publishing (`dry-run`, on by default).

All third-party actions are pinned to commit SHAs, workflows default to
`permissions: contents: read`, and only the `publish` job is granted write
access.

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

1. **Format and lint your code**
   Always run:

   ```bash
   make format
   make quality
   ```

   Installing the hooks once (`make hooks-install`) does this automatically for
   staged files.

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

4. Format and check your code:

   ```bash
   make format
   make quality
   ```

5. Ensure the project builds and passes its tests:

   ```bash
   make validate
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
