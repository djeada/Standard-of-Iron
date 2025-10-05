# Standard of Iron - Makefile
# Provides standard targets for building, running, and managing the project

# Default target
.DEFAULT_GOAL := help

# Configuration
BUILD_DIR := build
BINARY_NAME := standard_of_iron
MAP_EDITOR_BINARY := map_editor

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
	@echo "  $(GREEN)install$(RESET)     - Install all dependencies"
	@echo "  $(GREEN)configure$(RESET)   - Configure build with CMake"
	@echo "  $(GREEN)build$(RESET)       - Build the project"
	@echo "  $(GREEN)run$(RESET)         - Run the main application"
	@echo "  $(GREEN)editor$(RESET)      - Run the map editor"
	@echo "  $(GREEN)clean$(RESET)       - Clean build directory"
	@echo "  $(GREEN)rebuild$(RESET)     - Clean and build"
	@echo "  $(GREEN)test$(RESET)        - Run tests (if any)"
	@echo "  $(GREEN)format$(RESET)      - Format code and strip comments in ui/"
	@echo "  $(GREEN)check-deps$(RESET)  - Check if dependencies are installed"
	@echo "  $(GREEN)dev$(RESET)         - Set up development environment (install + configure + build)"
	@echo "  $(GREEN)all$(RESET)         - Full build (configure + build)"
	@echo ""
	@echo "$(BOLD)Examples:$(RESET)"
	@echo "  make install    # Install dependencies"
	@echo "  make dev        # Complete development setup"
	@echo "  make run        # Build and run the game"

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
	@cd $(BUILD_DIR) && cmake ..
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

# Format code (clang-format) THEN strip comments in ui/ via your script
.PHONY: format
format:
	@echo "$(BOLD)$(BLUE)Formatting code...$(RESET)"
	@if command -v clang-format >/dev/null 2>&1; then \
		find . -type f \( -name "*.cpp" -o -name "*.c" -o -name "*.h" -o -name "*.hpp" \) -not -path "./$(BUILD_DIR)/*" -exec clang-format -i {} +; \
		echo "$(GREEN)✓ Code formatted$(RESET)"; \
	else \
		echo "$(YELLOW)clang-format not found. Skipping code formatting.$(RESET)"; \
	fi
	@echo "$(BOLD)$(BLUE)Stripping comments in ui/...$(RESET)"
	@if [ -x scripts/remove-comments.sh ]; then \
		./scripts/remove-comments.sh ui/; \
	elif [ -f scripts/remove-comments.sh ]; then \
		bash scripts/remove-comments.sh ui/; \
	else \
		echo "$(RED)scripts/remove-comments.sh not found$(RESET)"; exit 1; \
	fi
	@echo "$(GREEN)✓ Format + comment strip complete$(RESET)"

# Debug build
.PHONY: debug
debug: build-dir
	@echo "$(BOLD)$(BLUE)Configuring debug build...$(RESET)"
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug ..
	@cd $(BUILD_DIR) && make -j$$(nproc)
	@echo "$(GREEN)✓ Debug build complete$(RESET)"

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
