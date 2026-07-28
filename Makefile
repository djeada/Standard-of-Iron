# Standard of Iron - Makefile
# Provides standard targets for building, running, and managing the project

# Default target
.DEFAULT_GOAL := help

# Configuration
BUILD_DIR := build
DEBUG_BUILD_DIR := build-debug
BUILD_TIDY_DIR := build-tidy
BINARY_NAME := standard_of_iron
MAP_EDITOR_BINARY := map_editor
ARENA_BINARY := arena_app
DEFAULT_LANG ?= en
MESH_OUTPUT_DIR ?= dist/mesh
SUPPORTED_MESH_SPECIES := horse elephant
MESH_SPECIES ?= $(word 2,$(MAKECMDGOALS))
MESH_PART ?= full

# Clang-tidy auto-fixer (git-only by default; --all scans whole project)
CLANG_TIDY_FIXER := scripts/run-clang-tidy-fixes.sh

# Optional knobs (override on the command line)
# e.g. make tidy CLANG_TIDY_JOBS=2 CLANG_TIDY_AUTO_FIX_CHECKS="-*,bugprone-*"
CLANG_TIDY_JOBS ?=
CLANG_TIDY_AUTO_FIX_CHECKS ?=
CLANG_TIDY_FIX_PATHS ?=
# Base for git diff (fallback is origin/main inside the script if unset)
CLANG_TIDY_GIT_BASE ?=

# ---- Formatting / linting driver ----
# scripts/format.py is the single entry point: the Makefile, the pre-commit
# hooks and CI all call it so they can never disagree about what "formatted"
# means. Tool versions are pinned in tools/versions.env.
PYTHON ?= python3
FORMAT_DRIVER := $(PYTHON) scripts/format.py
FORMAT_JOBS ?= $(shell command -v nproc >/dev/null 2>&1 && nproc || echo 4)
# Base revision used by the *-changed targets and by CI.
FORMAT_BASE ?= origin/main
# Extra flags forwarded to the driver (e.g. FORMAT_ARGS="--verbose").
FORMAT_ARGS ?=

# Colors for output
BOLD := \033[1m
GREEN := \033[32m
BLUE := \033[34m
YELLOW := \033[33m
RED := \033[31m
RESET := \033[0m

# Help target - shows available commands
.PHONY: help
help:
	@echo "$(BOLD)Standard of Iron - Build System$(RESET)"
	@echo ""
	@echo "$(BOLD)Available targets:$(RESET)"
	@echo "  $(GREEN)install$(RESET)       - Install all dependencies"
	@echo "  $(GREEN)configure$(RESET)     - Configure build with CMake"
	@echo "  $(GREEN)build$(RESET)         - Build the project"
	@echo "  $(GREEN)mesh$(RESET)          - Render creature mesh comparison sheet (use: make mesh horse)"
	@echo "  $(GREEN)build-tidy$(RESET)    - Build with clang-tidy static analysis enabled"
	@echo "  $(GREEN)debug$(RESET)         - Build with debug symbols, GDB support, and runtime tracing"
	@echo "  $(GREEN)release$(RESET)       - Build optimized release version"
	@echo "  $(GREEN)run$(RESET)           - Run the main application (includes map pipeline)"
	@echo "  $(GREEN)arena$(RESET)         - Run the arena playground tool"
	@echo "  $(GREEN)run-map-pipeline$(RESET) - Run map preprocessing pipeline only"
	@echo "  $(GREEN)editor$(RESET)        - Run the map editor"
	@echo "  $(GREEN)clean$(RESET)         - Clean build directory"
	@echo "  $(GREEN)rebuild$(RESET)       - Clean and build"
	@echo "  $(GREEN)test$(RESET)          - Run tests (if any)"
	@echo "  $(GREEN)validate-content$(RESET) - Validate mission and campaign JSON files"
	@echo "  $(GREEN)test-validator$(RESET) - Run validator integration tests"
	@echo "  $(GREEN)check-deps$(RESET)    - Check if dependencies are installed"
	@echo "  $(GREEN)dev$(RESET)           - Set up development environment (install + configure + build)"
	@echo "  $(GREEN)all$(RESET)           - Full build (configure + build)"
	@echo ""
	@echo "$(BOLD)Formatting (also strips comments via remove-comments.sh):$(RESET)"
	@echo "  $(GREEN)format$(RESET)        - Format, lint, verify, and check quality markers"
	@echo "  $(GREEN)format-check$(RESET)  - Verify formatting, change nothing (CI gate)"
	@echo "  $(GREEN)format-changed$(RESET) - Format only files changed vs FORMAT_BASE"
	@echo "  $(GREEN)format-check-changed$(RESET) - Fast check of changed files only"
	@echo "  $(GREEN)format-doctor$(RESET) - Report installed vs pinned tool versions"
	@echo "  $(GREEN)format-bootstrap$(RESET) - Install the pinned formatting toolchain"
	@echo ""
	@echo "$(BOLD)Linting and quality (also strips comments via remove-comments.sh):$(RESET)"
	@echo "  $(GREEN)lint$(RESET)          - clang-tidy, qmllint, Ruff, ShellCheck, yamllint, JSON"
	@echo "  $(GREEN)lint-fix$(RESET)      - Apply the linters' automated fixes (explicit)"
	@echo "  $(GREEN)lint-changed$(RESET)  - Lint only files changed vs FORMAT_BASE"
	@echo "  $(GREEN)quality$(RESET)       - lint + format-check + quality markers"
	@echo "  $(GREEN)validate$(RESET)      - quality + build + test + content validation"
	@echo "  $(GREEN)hooks-install$(RESET) - Install the pre-commit git hooks"
	@echo "  $(GREEN)tidy$(RESET)          - Run clang-tidy fixes on changed files"
	@echo "  $(GREEN)tidy-all$(RESET)      - Run clang-tidy fixes on the whole project"
	@echo "  $(RED)strip-comments$(RESET) - DESTRUCTIVE: delete comments (needs STRIP_COMMENTS_CONFIRM=1)"
	@echo ""
	@echo "$(BOLD)Examples:$(RESET)"
	@echo "  make install    # Install dependencies"
	@echo "  make dev        # Complete development setup"
	@echo "  make debug      # Build for debugging with GDB"
	@echo "  make run        # Build and run the game"
	@echo "  make mesh horse # Render a four-view horse mesh comparison sheet"
	@echo "  DEFAULT_LANG=de make build   # Build with German as default language"
	@echo "  FORMAT_BASE=origin/develop make format-check-changed"
	@echo "  make strip-comments DRY_RUN=1 # Preview the destructive rewrite"

# Install dependencies
.PHONY: install
install:
	@echo "$(BOLD)$(BLUE)Installing dependencies...$(RESET)"
	@bash scripts/setup-deps.sh --yes
	@echo "$(GREEN)✓ Dependencies installed successfully$(RESET)"

# Check if dependencies are installed
.PHONY: check-deps
check-deps:
	@echo "$(BOLD)$(BLUE)Checking dependencies...$(RESET)"
	@bash scripts/setup-deps.sh --dry-run

# Create build directory
build-dir:
	@mkdir -p $(BUILD_DIR)

# Configure build with CMake
.PHONY: configure
configure: build-dir
	@echo "$(BOLD)$(BLUE)Configuring build with CMake...$(RESET)"
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then \
		CACHED_SRC=$$(grep '^CMAKE_HOME_DIRECTORY:INTERNAL=' "$(BUILD_DIR)/CMakeCache.txt" | cut -d= -f2-); \
		if [ "$$CACHED_SRC" != "$$(pwd)" ]; then \
			echo "$(YELLOW)Build cache points at $$CACHED_SRC; resetting stale build tree$(RESET)"; \
			find "$(BUILD_DIR)" -mindepth 1 -maxdepth 1 -exec rm -rf {} +; \
		fi; \
	fi
	@if [ ! -f "$(BUILD_DIR)/_deps/googletest-src/googletest/include/gtest/gtest.h" ] || \
		[ ! -f "$(BUILD_DIR)/_deps/googletest-src/googletest/include/gtest/gtest-assertion-result.h" ] || \
		[ ! -f "$(BUILD_DIR)/_deps/googletest-src/googlemock/include/gmock/gmock.h" ]; then \
		echo "$(YELLOW)Detected incomplete googletest cache; refreshing build/_deps/googletest-*$(RESET)"; \
		rm -rf "$(BUILD_DIR)/_deps/googletest-src" \
		       "$(BUILD_DIR)/_deps/googletest-build" \
		       "$(BUILD_DIR)/_deps/googletest-subbuild"; \
	else \
		rm -rf "$(BUILD_DIR)/_deps/googletest-subbuild"; \
	fi
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_CLANG_TIDY=OFF -DDEFAULT_LANG=$(DEFAULT_LANG) ..
	@echo "$(GREEN)✓ Configuration complete$(RESET)"

# Build the project
.PHONY: build
build: run-map-pipeline configure
	@echo "$(BOLD)$(BLUE)Building project...$(RESET)"
	@cd $(BUILD_DIR) && make -j$$(nproc)
	@$(MAKE) bake-bpat
	@echo "$(GREEN)✓ Build complete$(RESET)"

# Build only the runnable application and runtime assets.
.PHONY: build-app
build-app: run-map-pipeline configure
	@echo "$(BOLD)$(BLUE)Building Standard of Iron...$(RESET)"
	@cd $(BUILD_DIR) && cmake --build . -j$$(nproc) --target $(BINARY_NAME)
	@$(MAKE) bake-bpat
	@echo "$(GREEN)✓ Application build complete$(RESET)"

# Bake creature animation textures (BPAT) into assets/creatures/.
# Runs after build so the bpat_baker binary exists. Idempotent.
.PHONY: bake-bpat
bake-bpat:
	@echo "$(BOLD)$(BLUE)Baking creature animation textures (BPAT)...$(RESET)"
	@$(BUILD_DIR)/bin/bpat_baker assets/creatures
	@echo "$(GREEN)✓ BPAT baking complete$(RESET)"

# Build with clang-tidy enabled
.PHONY: build-tidy
build-tidy:
	@echo "$(BOLD)$(BLUE)Configuring build with clang-tidy enabled...$(RESET)"
	@mkdir -p $(BUILD_TIDY_DIR)
	@cd $(BUILD_TIDY_DIR) && cmake -DENABLE_CLANG_TIDY=ON -DDEFAULT_LANG=$(DEFAULT_LANG) ..
	@echo "$(BOLD)$(BLUE)Building with clang-tidy analysis...$(RESET)"
	@cd $(BUILD_TIDY_DIR) && make -j$$(nproc)
	@echo "$(GREEN)✓ Build with clang-tidy complete$(RESET)"

# Build everything (alias for build)
.PHONY: all
all: build

.PHONY: horse elephant
horse elephant:
	@:

.PHONY: mesh
mesh: configure
	@case "$(MESH_SPECIES)" in \
		horse|elephant) ;; \
		*) echo "$(RED)Usage: make mesh [horse|elephant] MESH_PART=<full|torso|legs|neck_head>$(RESET)"; exit 2 ;; \
	esac
	@echo "$(BOLD)$(BLUE)Rendering $(MESH_SPECIES) mesh comparison views ($(MESH_PART))...$(RESET)"
	@mkdir -p "$(MESH_OUTPUT_DIR)"
	@cmake --build $(BUILD_DIR) -j$$(nproc) --target mesh_preview
	@QT_QPA_PLATFORM=offscreen "$(BUILD_DIR)/bin/mesh_preview" "$(MESH_SPECIES)" "$(MESH_OUTPUT_DIR)" "$(MESH_PART)"
	@echo "$(GREEN)✓ Mesh sheet written under $(MESH_OUTPUT_DIR)$(RESET)"

# Run map pipeline preprocessing
.PHONY: run-map-pipeline
run-map-pipeline:
	@echo "$(BOLD)$(BLUE)Running map pipeline preprocessing...$(RESET)"
	@bash scripts/run-map-pipeline.sh $(if $(map_pipeline_rebuild),--rebuild,)
	@echo "$(GREEN)✓ Map pipeline complete$(RESET)"

# Run the main application
.PHONY: run
run: build-app
	@echo "$(BOLD)$(BLUE)Running Standard of Iron...$(RESET)"
	@python3 scripts/purge-stale-saves.py "$(CURDIR)"
	@cd $(BUILD_DIR) && \
	BIN_PATH="./bin/$(BINARY_NAME)"; \
	if [ ! -x "$$BIN_PATH" ]; then \
		echo "$(RED)$(BINARY_NAME) not found at $$BIN_PATH$(RESET)"; \
		exit 127; \
	fi; \
	PLATFORM="$$(uname -s)"; \
	DEFAULT_QPA="offscreen"; \
	case "$$PLATFORM" in \
		Darwin) DEFAULT_QPA="cocoa" ;; \
		MINGW*|MSYS*|CYGWIN*) DEFAULT_QPA="windows" ;; \
		*) \
			if [ -n "$$WAYLAND_DISPLAY" ]; then \
				DEFAULT_QPA="wayland"; \
			elif [ -n "$$DISPLAY" ]; then \
				DEFAULT_QPA="xcb"; \
			fi ;; \
	esac; \
	if [ -z "$$QT_QPA_PLATFORM" ]; then \
		echo "$(YELLOW)QT_QPA_PLATFORM not set; defaulting to $$DEFAULT_QPA$(RESET)"; \
		QT_QPA_PLATFORM="$$DEFAULT_QPA" "$${BIN_PATH}"; \
	else \
		"$${BIN_PATH}"; \
	fi

# Run with xvfb for headless environments (software rasterization)
.PHONY: run-headless
run-headless: build
	@echo "$(BOLD)$(BLUE)Running Standard of Iron under xvfb...$(RESET)"
	@if ! command -v xvfb-run >/dev/null 2>&1; then \
	  echo "$(YELLOW)xvfb-run not found. Installing...$(RESET)"; \
	  sudo apt-get update -y >/dev/null 2>&1 && sudo apt-get install -y xvfb >/dev/null 2>&1; \
	fi
	@cd $(BUILD_DIR) && \
	BIN_PATH="./bin/$(BINARY_NAME)"; \
	if [ ! -x "$$BIN_PATH" ]; then \
		echo "$(RED)$(BINARY_NAME) not found at $$BIN_PATH$(RESET)"; \
		exit 127; \
	fi; \
	xvfb-run -s "-screen 0 1280x720x24" "$$BIN_PATH"

# Run the map editor
.PHONY: editor
editor: run-map-pipeline configure
	@echo "$(BOLD)$(BLUE)Building Map Editor...$(RESET)"
	@cmake --build $(BUILD_DIR) -j$$(nproc) --target $(MAP_EDITOR_BINARY)
	@echo "$(BOLD)$(BLUE)Running Map Editor...$(RESET)"
	@cd $(BUILD_DIR) && ./bin/$(MAP_EDITOR_BINARY)

.PHONY: arena
arena: run-map-pipeline configure
	@echo "$(BOLD)$(BLUE)Building Arena playground...$(RESET)"
	@cmake --build $(BUILD_DIR) -j$$(nproc) --target $(ARENA_BINARY)
	@echo "$(BOLD)$(BLUE)Running Arena playground...$(RESET)"
	@cd $(BUILD_DIR) && \
	BIN_PATH="./bin/$(ARENA_BINARY)"; \
	if [ ! -x "$$BIN_PATH" ]; then \
		echo "$(RED)$(ARENA_BINARY) not found at $$BIN_PATH$(RESET)"; \
		exit 127; \
	fi; \
	PLATFORM="$$(uname -s)"; \
	DEFAULT_QPA="offscreen"; \
	case "$$PLATFORM" in \
		Darwin) DEFAULT_QPA="cocoa" ;; \
		MINGW*|MSYS*|CYGWIN*) DEFAULT_QPA="windows" ;; \
		*) \
			if [ -n "$$WAYLAND_DISPLAY" ]; then \
				DEFAULT_QPA="wayland"; \
			elif [ -n "$$DISPLAY" ]; then \
				DEFAULT_QPA="xcb"; \
			fi ;; \
	esac; \
	if [ -z "$$QT_QPA_PLATFORM" ]; then \
		echo "$(YELLOW)QT_QPA_PLATFORM not set; defaulting to $$DEFAULT_QPA$(RESET)"; \
		QT_QPA_PLATFORM="$$DEFAULT_QPA" "$${BIN_PATH}"; \
	else \
		"$${BIN_PATH}"; \
	fi

# Clean build directory
.PHONY: clean
clean:
	@echo "$(BOLD)$(YELLOW)Cleaning build directory...$(RESET)"
	@rm -rf $(BUILD_DIR) $(DEBUG_BUILD_DIR) $(BUILD_TIDY_DIR)
	@echo "$(GREEN)✓ Clean complete$(RESET)"

# Rebuild (clean + build)
.PHONY: rebuild
rebuild: clean build

# Development setup (install + configure + build)
.PHONY: dev
dev: install build
	@echo "$(GREEN)✓ Development environment ready!$(RESET)"
	@echo "$(BOLD)You can now run:$(RESET)"
	@echo "  make run      # Run the game"
	@echo "  make arena    # Run the arena playground"
	@echo "  make editor   # Run the map editor"

# Run tests (placeholder for future test implementation)
.PHONY: test
test: build
	@echo "$(BOLD)$(BLUE)Running tests...$(RESET)"
	@if [ -f "$(BUILD_DIR)/bin/standard_of_iron_tests" ]; then \
		./$(BUILD_DIR)/bin/standard_of_iron_tests; \
	else \
		echo "$(RED)Test executable not found. Build may have failed.$(RESET)"; \
		exit 1; \
	fi
	@# The QML design-system suite is skipped when Qt QuickTest is unavailable,
	@# so a missing binary is not a build failure here.
	@if [ -f "$(BUILD_DIR)/bin/design_system_qml_tests" ]; then \
		echo "$(BOLD)$(BLUE)Running design system QML tests...$(RESET)"; \
		QT_QPA_PLATFORM=offscreen ./$(BUILD_DIR)/bin/design_system_qml_tests \
			-input tests/ui/qml; \
	else \
		echo "$(YELLOW)⚠ design_system_qml_tests not built (Qt QuickTest missing). Skipping.$(RESET)"; \
	fi

# Validate mission and campaign content
.PHONY: validate-content
validate-content: build
	@echo "$(BOLD)$(BLUE)Validating mission and campaign content...$(RESET)"
	@if [ -f "$(BUILD_DIR)/bin/content_validator" ]; then \
		$(BUILD_DIR)/bin/content_validator assets; \
		if [ $$? -eq 0 ]; then \
			echo "$(GREEN)✓ Content validation passed$(RESET)"; \
		else \
			echo "$(RED)✗ Content validation failed$(RESET)"; \
			exit 1; \
		fi \
	else \
		echo "$(RED)Content validator not found. Build may have failed.$(RESET)"; \
		exit 1; \
	fi

# Test validator with integration tests
.PHONY: test-validator
test-validator: build
	@echo "$(BOLD)$(BLUE)Running validator integration tests...$(RESET)"
	@if [ -f "tests/validator_integration_test.sh" ]; then \
		bash tests/validator_integration_test.sh; \
	else \
		echo "$(RED)Validator integration test not found.$(RESET)"; \
		exit 1; \
	fi

# ---- Formatting (also strips comments via remove-comments.sh) ----
.PHONY: format format-check format-changed format-check-changed \
	format-staged format-doctor format-bootstrap clean-format-trash

## Format every tracked file, then run the complete non-compiler quality gate.
format: clean-format-trash
	@$(FORMAT_DRIVER) --all --strip-comments --fix --strict --jobs $(FORMAT_JOBS) $(FORMAT_ARGS)
	@$(MAKE) --no-print-directory quality

## Verify formatting without writing anything (CI gate).
format-check:
	@$(FORMAT_DRIVER) --all --check --jobs $(FORMAT_JOBS) $(FORMAT_ARGS)

## Format only what changed against FORMAT_BASE (default origin/main).
format-changed: clean-format-trash
	@$(FORMAT_DRIVER) --changed $(FORMAT_BASE) --fix --jobs $(FORMAT_JOBS) $(FORMAT_ARGS)

## Fast pull-request gate: check only the changed files.
format-check-changed:
	@$(FORMAT_DRIVER) --changed $(FORMAT_BASE) --check --jobs $(FORMAT_JOBS) $(FORMAT_ARGS)

## Format the files currently staged in the index.
format-staged:
	@$(FORMAT_DRIVER) --staged --fix --jobs $(FORMAT_JOBS) $(FORMAT_ARGS)

## Report which formatters are installed and whether they match the pins.
format-doctor:
	@$(FORMAT_DRIVER) --doctor

## Install the pinned toolchain (pip/npm) and print system-package hints.
format-bootstrap:
	@$(FORMAT_DRIVER) --bootstrap $(FORMAT_ARGS)

## Remove editor and qmlformat backup droppings.
clean-format-trash:
	@find . \
		\( -path "./.git" -o -path "./$(BUILD_DIR)" -o -path "./$(BUILD_DIR)/*" -o -path "./$(DEBUG_BUILD_DIR)" -o -path "./$(DEBUG_BUILD_DIR)/*" -o -path "./$(BUILD_TIDY_DIR)" -o -path "./$(BUILD_TIDY_DIR)/*" -o -path "./third_party" -o -path "./third_party/*" \) -prune -o \
		-type f \( -name "*~" -o -name ".#*" -o -name "#*#" \) -print -exec rm -f {} +

# ---- Linting (also strips comments via remove-comments.sh) ----
.PHONY: lint lint-fix lint-changed lint-deep

## Run every linter: clang-tidy, qmllint, Ruff, ShellCheck, yamllint, JSON.
lint:
	@$(FORMAT_DRIVER) --all --strip-comments --lint --jobs $(FORMAT_JOBS) --build-dir $(BUILD_DIR) $(FORMAT_ARGS)

## Apply the linters' automated fixes. Explicit and separate from `format`.
lint-fix:
	@$(FORMAT_DRIVER) --all --lint --fix --jobs $(FORMAT_JOBS) --build-dir $(BUILD_DIR) $(FORMAT_ARGS)

## Lint only what changed against FORMAT_BASE (pull-request gate).
lint-changed:
	@$(FORMAT_DRIVER) --changed $(FORMAT_BASE) --lint --jobs $(FORMAT_JOBS) --build-dir $(BUILD_DIR) $(FORMAT_ARGS)

## Nightly lane: whole-project clang-tidy, advisory findings become failures.
lint-deep:
	@$(FORMAT_DRIVER) --all --lint --deep --fail-on-advisory --jobs $(FORMAT_JOBS) --build-dir $(BUILD_DIR) $(FORMAT_ARGS)

# ---- Destructive source transformations (never run by `format`) ----
.PHONY: strip-comments format-strip-comments

## DESTRUCTIVE: delete comments from tracked sources. Review the diff.
strip-comments:
	@echo "$(BOLD)$(RED)This rewrites source files and deletes comments.$(RESET)"
	@echo "$(YELLOW)Set STRIP_COMMENTS_CONFIRM=1 to proceed, or run with DRY_RUN=1.$(RESET)"
	@if [ "$(DRY_RUN)" = "1" ]; then \
		$(FORMAT_DRIVER) --all --strip-comments --dry-run; \
	elif [ "$(STRIP_COMMENTS_CONFIRM)" = "1" ]; then \
		$(FORMAT_DRIVER) --all --strip-comments; \
	else \
		echo "$(RED)Refusing to strip comments without STRIP_COMMENTS_CONFIRM=1$(RESET)"; \
		exit 1; \
	fi

# Backwards-compatible alias for the old target name.
format-strip-comments: strip-comments

# ---- Aggregate gates ----
.PHONY: quality validate hooks-install

## Everything that does not need a compiler: comment stripping, linting, formatting, markers.
quality:
	@$(MAKE) --no-print-directory lint FORMAT_ARGS="$(FORMAT_ARGS) --strict"
	@$(MAKE) --no-print-directory format-check FORMAT_ARGS="$(FORMAT_ARGS) --strict"
	@echo "$(BOLD)$(BLUE)Checking quality markers...$(RESET)"
	@git ls-files -z '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hpp' '*.py' '*.sh' \
		| xargs -0 -r $(PYTHON) scripts/check-quality-markers.py
	@echo "$(GREEN)✓ Quality checks passed$(RESET)"

## Full local pre-push gate: quality + build + tests + content validation.
validate: quality build test validate-content
	@echo "$(BOLD)$(BLUE)Validating shader uniforms...$(RESET)"
	@$(PYTHON) scripts/validate_shader_uniforms.py
	@echo "$(BOLD)$(BLUE)Validating OpenGL requirements...$(RESET)"
	@$(PYTHON) scripts/validate_opengl_requirements.py
	@echo "$(GREEN)✓ Validation complete$(RESET)"

## Install the git hooks defined in .pre-commit-config.yaml.
hooks-install:
	@if command -v pre-commit >/dev/null 2>&1; then \
		pre-commit install --install-hooks; \
		echo "$(GREEN)✓ Git hooks installed$(RESET)"; \
	else \
		echo "$(RED)pre-commit not found. Run 'make format-bootstrap' first.$(RESET)"; \
		exit 1; \
	fi

# ---- clang-tidy auto-fixer (heavier, explicit) ----
.PHONY: tidy tidy-all

tidy:
	@echo "$(BOLD)$(BLUE)Running clang-tidy fixes on changed files (vs $${CLANG_TIDY_GIT_BASE:-origin/main})...$(RESET)"
	@bash $(CLANG_TIDY_FIXER) \
		--nice \
		--build-dir="$(BUILD_DIR)" \
		--default-lang="$(DEFAULT_LANG)" \
		$(if $(CLANG_TIDY_JOBS),--jobs="$(CLANG_TIDY_JOBS)") \
		$(if $(CLANG_TIDY_FIX_PATHS),--paths="$(CLANG_TIDY_FIX_PATHS)") \
		$(if $(CLANG_TIDY_AUTO_FIX_CHECKS),--checks="$(CLANG_TIDY_AUTO_FIX_CHECKS)")

tidy-all:
	@echo "$(BOLD)$(BLUE)Running clang-tidy fixes on ALL source files...$(RESET)"
	@bash $(CLANG_TIDY_FIXER) \
		--all \
		--nice \
		--build-dir="$(BUILD_DIR)" \
		--default-lang="$(DEFAULT_LANG)" \
		$(if $(CLANG_TIDY_JOBS),--jobs="$(CLANG_TIDY_JOBS)") \
		$(if $(CLANG_TIDY_FIX_PATHS),--paths="$(CLANG_TIDY_FIX_PATHS)") \
		$(if $(CLANG_TIDY_AUTO_FIX_CHECKS),--checks="$(CLANG_TIDY_AUTO_FIX_CHECKS)")

# Debug build
.PHONY: debug
debug:
	@echo "$(BOLD)$(BLUE)Configuring debug build with GDB support and runtime tracing...$(RESET)"
	@mkdir -p $(DEBUG_BUILD_DIR)
	@cd $(DEBUG_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_CLANG_TIDY=OFF -DDEFAULT_LANG=$(DEFAULT_LANG) ..
	@echo "$(BOLD)$(BLUE)Building debug version...$(RESET)"
	@cd $(DEBUG_BUILD_DIR) && make -j$$(nproc)
	@echo "$(GREEN)✓ Debug build complete$(RESET)"
	@echo "$(BOLD)Debug Info:$(RESET)"
	@echo "  Debug symbols: $(GREEN)Enabled (-g3 -ggdb3)$(RESET)"
	@echo "  Optimizations: $(YELLOW)Disabled (-O0)$(RESET)"
	@echo "  Frame pointers: $(GREEN)Preserved$(RESET)"
	@echo "  Inlining: $(YELLOW)Disabled$(RESET)"
	@echo ""
	@echo "$(BOLD)Run with GDB:$(RESET)"
	@echo "  cd $(DEBUG_BUILD_DIR) && gdb ./bin/$(BINARY_NAME)"
	@echo "  cd $(DEBUG_BUILD_DIR) && gdb --args ./bin/$(BINARY_NAME) [args]"

# Release build
.PHONY: release
release: build-dir
	@echo "$(BOLD)$(BLUE)Configuring release build...$(RESET)"
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_CLANG_TIDY=OFF ..
	@cd $(BUILD_DIR) && make -j$$(nproc)
	@echo "$(GREEN)✓ Release build complete$(RESET)"

# Show build info
.PHONY: info
info:
	@echo "$(BOLD)Project Information:$(RESET)"
	@echo "  Build directory: $(BUILD_DIR)"
	@echo "  Binary name: $(BINARY_NAME)"
	@echo "  Map editor: $(MAP_EDITOR_BINARY)"
	@echo "  CMake version: $$(cmake --version | head -1)"
	@echo "  GCC version: $$(gcc --version | head -1)"
	@if [ -f "$(BUILD_DIR)/$(BINARY_NAME)" ]; then \
		echo "  Binary built: $(GREEN)✓$(RESET)"; \
	else \
		echo "  Binary built: $(RED)✗$(RESET)"; \
	fi

# Quick start for new developers
.PHONY: quickstart
quickstart:
	@echo "$(BOLD)$(GREEN)Quick Start Guide:$(RESET)"
	@echo "1. Install dependencies: $(BLUE)make install$(RESET)"
	@echo "2. Build the project: $(BLUE)make build$(RESET)"
	@echo "3. Run the game: $(BLUE)make run$(RESET)"
	@echo "4. Run the arena playground: $(BLUE)make arena$(RESET)"
	@echo ""
	@echo "Or use the shortcut: $(BLUE)make dev$(RESET)"
