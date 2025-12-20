# Standard of Iron - Makefile
# Provides standard targets for building, running, and managing the project

# Default target
.DEFAULT_GOAL := help

# Configuration
BUILD_DIR := build
BUILD_TIDY_DIR := build-tidy
BINARY_NAME := standard_of_iron
MAP_EDITOR_BINARY := map_editor
DEFAULT_LANG ?= en

# Clang-tidy auto-fixer (git-only by default; --all scans whole project)
CLANG_TIDY_FIXER := scripts/run-clang-tidy-fixes.sh

# Optional knobs (override on the command line)
# e.g. make tidy CLANG_TIDY_JOBS=2 CLANG_TIDY_AUTO_FIX_CHECKS="-*,bugprone-*"
CLANG_TIDY_JOBS ?=
CLANG_TIDY_AUTO_FIX_CHECKS ?=
CLANG_TIDY_FIX_PATHS ?=
# Base for git diff (fallback is origin/main inside the script if unset)
CLANG_TIDY_GIT_BASE ?=

# Formatting config
CLANG_FORMAT ?= clang-format
# Try to find qmlformat in common Qt installation paths if not in PATH
QMLFORMAT ?= $(shell command -v qmlformat 2>/dev/null || echo /usr/lib/qt5/bin/qmlformat)
FMT_GLOBS := -name "*.cpp" -o -name "*.c" -o -name "*.h" -o -name "*.hpp"
SHADER_GLOBS := -name "*.frag" -o -name "*.vert"
QML_GLOBS := -name "*.qml"

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
	@echo "  $(GREEN)build-tidy$(RESET)    - Build with clang-tidy static analysis enabled"
	@echo "  $(GREEN)debug$(RESET)         - Build with debug symbols and GDB support (no optimizations)"
	@echo "  $(GREEN)release$(RESET)       - Build optimized release version"
	@echo "  $(GREEN)run$(RESET)           - Run the main application (includes map pipeline)"
	@echo "  $(GREEN)run-map-pipeline$(RESET) - Run map preprocessing pipeline only"
	@echo "  $(GREEN)editor$(RESET)        - Run the map editor"
	@echo "  $(GREEN)clean$(RESET)         - Clean build directory"
	@echo "  $(GREEN)rebuild$(RESET)       - Clean and build"
	@echo "  $(GREEN)test$(RESET)          - Run tests (if any)"
	@echo "  $(GREEN)validate-content$(RESET) - Validate mission and campaign JSON files"
	@echo "  $(GREEN)test-validator$(RESET) - Run validator integration tests"
	@echo "  $(GREEN)format$(RESET)        - Format all code (C++, QML, shaders)"
	@echo "  $(GREEN)format-check$(RESET)  - Verify formatting (CI-friendly, no changes)"
	@echo "  $(GREEN)tidy$(RESET)          - Run clang-tidy fixes on changed files (git diff vs origin/main)"
	@echo "  $(GREEN)tidy-all$(RESET)      - Run clang-tidy fixes on the whole project"
	@echo "  $(GREEN)check-deps$(RESET)    - Check if dependencies are installed"
	@echo "  $(GREEN)dev$(RESET)           - Set up development environment (install + configure + build)"
	@echo "  $(GREEN)all$(RESET)           - Full build (configure + build)"
	@echo ""
	@echo "$(BOLD)Examples:$(RESET)"
	@echo "  make install    # Install dependencies"
	@echo "  make dev        # Complete development setup"
	@echo "  make debug      # Build for debugging with GDB"
	@echo "  make run        # Build and run the game"
	@echo "  DEFAULT_LANG=de make build  # Build with German as default language"

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
	@cd $(BUILD_DIR) && cmake -DENABLE_CLANG_TIDY=OFF -DDEFAULT_LANG=$(DEFAULT_LANG) ..
	@echo "$(GREEN)✓ Configuration complete$(RESET)"

# Build the project
.PHONY: build
build: configure
	@echo "$(BOLD)$(BLUE)Building project...$(RESET)"
	@cd $(BUILD_DIR) && make -j$$(nproc)
	@echo "$(GREEN)✓ Build complete$(RESET)"

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

# Run map pipeline preprocessing
.PHONY: run-map-pipeline
run-map-pipeline:
	@echo "$(BOLD)$(BLUE)Running map pipeline preprocessing...$(RESET)"
	@bash scripts/run-map-pipeline.sh $(if $(map_pipeline_rebuild),--rebuild,)
	@echo "$(GREEN)✓ Map pipeline complete$(RESET)"

# Run the main application
.PHONY: run
run: run-map-pipeline build
	@echo "$(BOLD)$(BLUE)Running Standard of Iron...$(RESET)"
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
editor: build
	@echo "$(BOLD)$(BLUE)Running Map Editor...$(RESET)"
	@cd $(BUILD_DIR) && ./tools/map_editor/$(MAP_EDITOR_BINARY)

# Clean build directory
.PHONY: clean
clean:
	@echo "$(BOLD)$(YELLOW)Cleaning build directory...$(RESET)"
	@rm -rf $(BUILD_DIR) $(BUILD_TIDY_DIR)
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
	@echo "  make editor   # Run the map editor"

# Run tests (placeholder for future test implementation)
.PHONY: test
test: build
	@echo "$(BOLD)$(BLUE)Running tests...$(RESET)"
	@if [ -f "$(BUILD_DIR)/bin/standard_of_iron_tests" ]; then \
		cd $(BUILD_DIR) && ./bin/standard_of_iron_tests; \
	else \
		echo "$(RED)Test executable not found. Build may have failed.$(RESET)"; \
		exit 1; \
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

# ---- Formatting: strip comments first, then format (strict) ----
.PHONY: format format-check

EXCLUDE_DIRS := ./$(BUILD_DIR) ./$(BUILD_TIDY_DIR) ./third_party
EXCLUDE_FIND := $(foreach d,$(EXCLUDE_DIRS),-not -path "$(d)/*")

format:
	@echo "$(BOLD)$(BLUE)Stripping comments in app/... game/... render/... tools/... ui/... assets/shaders/...$(RESET)"
	@if [ -x scripts/remove-comments.sh ]; then \
		./scripts/remove-comments.sh app/ game/ render/ tools/ ui/ assets/shaders/; \
	elif [ -f scripts/remove-comments.sh ]; then \
		bash scripts/remove-comments.sh app/ game/ render/ tools/ ui/ assets/shaders/; \
	else \
		echo "$(RED)scripts/remove-comments.sh not found$(RESET)"; exit 1; \
	fi

	@echo "$(BOLD)$(BLUE)Applying clang-tidy auto fixes (git-only, nice)...$(RESET)"
	@bash $(CLANG_TIDY_FIXER) --nice --build-dir="$(BUILD_DIR)" --default-lang="$(DEFAULT_LANG)" $(if $(CLANG_TIDY_AUTO_FIX_CHECKS),--checks="$(CLANG_TIDY_AUTO_FIX_CHECKS)")

	@echo "$(BOLD)$(BLUE)Formatting C/C++ files with clang-format...$(RESET)"
	@if command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		find . -type f \( $(FMT_GLOBS) \) $(EXCLUDE_FIND) -print0 \
		| xargs -0 -r $(CLANG_FORMAT) -i --style=file; \
		echo "$(GREEN)✓ C/C++ formatting complete$(RESET)"; \
	else \
		echo "$(RED)clang-format not found. Please install it.$(RESET)"; exit 1; \
	fi

	@echo "$(BOLD)$(BLUE)Formatting QML files...$(RESET)"
	@if command -v $(QMLFORMAT) >/dev/null 2>&1 || [ -x "$(QMLFORMAT)" ]; then \
		find . -type f \( $(QML_GLOBS) \) $(EXCLUDE_FIND) -print0 \
		| xargs -0 -r $(QMLFORMAT) -i; \
		echo "$(GREEN)✓ QML formatting complete$(RESET)"; \
	else \
		echo "$(YELLOW)⚠ qmlformat not found. Skipping QML formatting.$(RESET)"; \
	fi

	@echo "$(BOLD)$(BLUE)Formatting shader files (.frag, .vert)...$(RESET)"
	@if command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		find . -type f \( $(SHADER_GLOBS) \) $(EXCLUDE_FIND) -print0 \
		| xargs -0 -r $(CLANG_FORMAT) -i --style=file; \
		echo "$(GREEN)✓ Shader formatting complete$(RESET)"; \
	else \
		echo "$(YELLOW)⚠ clang-format not found. Shader files not formatted.$(RESET)"; \
	fi

	@echo "$(GREEN)✓ All formatting complete$(RESET)"

format-check:
	@echo "$(BOLD)$(BLUE)Checking formatting compliance...$(RESET)"
	@FAILED=0; \
	if command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		echo "$(BLUE)Checking C/C++ files...$(RESET)"; \
		find . -type f \( $(FMT_GLOBS) \) $(EXCLUDE_FIND) -print0 \
		| xargs -0 -r $(CLANG_FORMAT) --dry-run -Werror --style=file || FAILED=1; \
		echo "$(BLUE)Checking shader files...$(RESET)"; \
		find . -type f \( $(SHADER_GLOBS) \) $(EXCLUDE_FIND) -print0 \
		| xargs -0 -r $(CLANG_FORMAT) --dry-run -Werror --style=file || FAILED=1; \
	fi; \
	if command -v $(QMLFORMAT) >/dev/null 2>&1 || [ -x "$(QMLFORMAT)" ]; then \
		echo "$(BLUE)Checking QML files...$(RESET)"; \
		for file in $$(find . -type f \( $(QML_GLOBS) \) $(EXCLUDE_FIND)); do \
			$(QMLFORMAT) "$$file" > /tmp/qmlformat_check.tmp 2>/dev/null; \
			if ! diff -q "$$file" /tmp/qmlformat_check.tmp >/dev/null 2>&1; then \
				echo "$(RED)QML file needs formatting: $$file$(RESET)"; \
				FAILED=1; \
			fi; \
		done; \
		rm -f /tmp/qmlformat_check.tmp; \
	fi; \
	if [ $$FAILED -eq 0 ]; then \
		echo "$(GREEN)✓ All formatting checks passed$(RESET)"; \
	else \
		echo "$(RED)✗ Formatting check failed. Run 'make format' to fix.$(RESET)"; \
		exit 1; \
	fi

# ---- Static analysis: clang-tidy (driven by fixer script) ----
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
debug: build-dir
	@echo "$(BOLD)$(BLUE)Configuring debug build with GDB support...$(RESET)"
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_CLANG_TIDY=OFF -DDEFAULT_LANG=$(DEFAULT_LANG) ..
	@echo "$(BOLD)$(BLUE)Building debug version...$(RESET)"
	@cd $(BUILD_DIR) && make -j$$(nproc)
	@echo "$(GREEN)✓ Debug build complete$(RESET)"
	@echo "$(BOLD)Debug Info:$(RESET)"
	@echo "  Debug symbols: $(GREEN)Enabled (-g3 -ggdb3)$(RESET)"
	@echo "  Optimizations: $(YELLOW)Disabled (-O0)$(RESET)"
	@echo "  Frame pointers: $(GREEN)Preserved$(RESET)"
	@echo "  Inlining: $(YELLOW)Disabled$(RESET)"
	@echo ""
	@echo "$(BOLD)Run with GDB:$(RESET)"
	@echo "  cd $(BUILD_DIR) && gdb ./$(BINARY_NAME)"
	@echo "  cd $(BUILD_DIR) && gdb --args ./$(BINARY_NAME) [args]"

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
	@echo ""
	@echo "Or use the shortcut: $(BLUE)make dev$(RESET)"
