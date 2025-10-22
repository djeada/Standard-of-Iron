# Standard of Iron - Makefile
# Provides standard targets for building, running, and managing the project

# Default target
.DEFAULT_GOAL := help

# Configuration
BUILD_DIR := build
BINARY_NAME := standard_of_iron
MAP_EDITOR_BINARY := map_editor
DEFAULT_LANG ?= en

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
	@echo "  $(GREEN)debug$(RESET)         - Build with debug symbols and GDB support (no optimizations)"
	@echo "  $(GREEN)release$(RESET)       - Build optimized release version"
	@echo "  $(GREEN)run$(RESET)           - Run the main application"
	@echo "  $(GREEN)editor$(RESET)        - Run the map editor"
	@echo "  $(GREEN)clean$(RESET)         - Clean build directory"
	@echo "  $(GREEN)rebuild$(RESET)       - Clean and build"
	@echo "  $(GREEN)test$(RESET)          - Run tests (if any)"
	@echo "  $(GREEN)format$(RESET)        - Format all code (C++, QML, shaders)"
	@echo "  $(GREEN)format-check$(RESET)  - Verify formatting (CI-friendly, no changes)"
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
	@cd $(BUILD_DIR) && cmake -DDEFAULT_LANG=$(DEFAULT_LANG) ..
	@echo "$(GREEN)✓ Configuration complete$(RESET)"

# Build the project
.PHONY: build
build: configure
	@echo "$(BOLD)$(BLUE)Building project...$(RESET)"
	@cd $(BUILD_DIR) && make -j$$(nproc)
	@echo "$(GREEN)✓ Build complete$(RESET)"

# Build everything (alias for build)
.PHONY: all
all: build

# Run the main application
.PHONY: run
run: build
	@echo "$(BOLD)$(BLUE)Running Standard of Iron...$(RESET)"
	@cd $(BUILD_DIR) && \
	if [ -n "$$DISPLAY" ]; then \
	  ./$(BINARY_NAME); \
	else \
	  echo "$(YELLOW)No DISPLAY detected; running with QT_QPA_PLATFORM=offscreen$(RESET)"; \
	  QT_QPA_PLATFORM=offscreen ./$(BINARY_NAME); \
	fi

# Run with xvfb for headless environments (software rasterization)
.PHONY: run-headless
run-headless: build
	@echo "$(BOLD)$(BLUE)Running Standard of Iron under xvfb...$(RESET)"
	@if ! command -v xvfb-run >/dev/null 2>&1; then \
	  echo "$(YELLOW)xvfb-run not found. Installing...$(RESET)"; \
	  sudo apt-get update -y >/dev/null 2>&1 && sudo apt-get install -y xvfb >/dev/null 2>&1; \
	fi
	@cd $(BUILD_DIR) && xvfb-run -s "-screen 0 1280x720x24" ./$(BINARY_NAME)

# Run the map editor
.PHONY: editor
editor: build
	@echo "$(BOLD)$(BLUE)Running Map Editor...$(RESET)"
	@cd $(BUILD_DIR) && ./tools/map_editor/$(MAP_EDITOR_BINARY)

# Clean build directory
.PHONY: clean
clean:
	@echo "$(BOLD)$(YELLOW)Cleaning build directory...$(RESET)"
	@rm -rf $(BUILD_DIR)
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
	@if [ -f "$(BUILD_DIR)/tests" ]; then \
		cd $(BUILD_DIR) && ./tests; \
	else \
		echo "$(YELLOW)No tests found. Test suite not yet implemented.$(RESET)"; \
	fi

# ---- Formatting: strip comments first, then format (strict) ----
.PHONY: format format-check

EXCLUDE_DIRS := ./$(BUILD_DIR) ./third_party
EXCLUDE_FIND := $(foreach d,$(EXCLUDE_DIRS),-not -path "$(d)/*")

format:
	@echo "$(BOLD)$(BLUE)Stripping comments in app/... game/... render/... tools/... ui/...$(RESET)"
	@if [ -x scripts/remove-comments.sh ]; then \
		./scripts/remove-comments.sh app/ game/ render/ tools/ ui/; \
	elif [ -f scripts/remove-comments.sh ]; then \
		bash scripts/remove-comments.sh app/ game/ render/ tools/ ui/; \
	else \
		echo "$(RED)scripts/remove-comments.sh not found$(RESET)"; exit 1; \
	fi

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
		echo "$(YELLOW)  Install qmlformat (from Qt dev tools) to format QML files.$(RESET)"; \
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

# CI/verification: fail if anything would be reformatted
format-check:
	@echo "$(BOLD)$(BLUE)Checking formatting compliance...$(RESET)"
	@FAILED=0; \
	if command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		echo "$(BLUE)Checking C/C++ files...$(RESET)"; \
		find . -type f \( $(FMT_GLOBS) \) -not -path "./$(BUILD_DIR)/*" -print0 \
		| xargs -0 -r $(CLANG_FORMAT) --dry-run -Werror --style=file || FAILED=1; \
		echo "$(BLUE)Checking shader files...$(RESET)"; \
		find . -type f \( $(SHADER_GLOBS) \) -not -path "./$(BUILD_DIR)/*" -print0 \
		| xargs -0 -r $(CLANG_FORMAT) --dry-run -Werror --style=file || FAILED=1; \
	else \
		echo "$(RED)clang-format not found. Please install it.$(RESET)"; exit 1; \
	fi; \
	if command -v $(QMLFORMAT) >/dev/null 2>&1 || [ -x "$(QMLFORMAT)" ]; then \
		echo "$(BLUE)Checking QML files...$(RESET)"; \
		for file in $$(find . -type f \( $(QML_GLOBS) \) -not -path "./$(BUILD_DIR)/*"); do \
			$(QMLFORMAT) "$$file" > /tmp/qmlformat_check.tmp 2>/dev/null; \
			if ! diff -q "$$file" /tmp/qmlformat_check.tmp >/dev/null 2>&1; then \
				echo "$(RED)QML file needs formatting: $$file$(RESET)"; \
				FAILED=1; \
			fi; \
		done; \
		rm -f /tmp/qmlformat_check.tmp; \
	else \
		echo "$(YELLOW)⚠ qmlformat not found. Skipping QML format check.$(RESET)"; \
	fi; \
	if [ $$FAILED -eq 0 ]; then \
		echo "$(GREEN)✓ All formatting checks passed$(RESET)"; \
	else \
		echo "$(RED)✗ Formatting check failed. Run 'make format' to fix.$(RESET)"; \
		exit 1; \
	fi

# Debug build
.PHONY: debug
debug: build-dir
	@echo "$(BOLD)$(BLUE)Configuring debug build with GDB support...$(RESET)"
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DDEFAULT_LANG=$(DEFAULT_LANG) ..
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
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release ..
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

