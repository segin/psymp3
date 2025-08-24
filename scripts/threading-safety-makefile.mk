# Threading Safety Makefile Integration
# Include this in your main Makefile to add threading safety checks
#
# Usage:
#   include scripts/threading-safety-makefile.mk
#   make threading-check          # Run basic threading safety checks
#   make threading-check-strict   # Run strict checks (warnings as errors)
#   make threading-analysis       # Run full analysis with report generation

# Threading safety check targets
.PHONY: threading-check threading-check-strict threading-analysis threading-clean

# Basic threading safety check (errors only)
threading-check:
	@echo "Running threading safety pattern checks..."
	@python3 scripts/check_threading_patterns.py

# Strict threading safety check (warnings as errors)
threading-check-strict:
	@echo "Running strict threading safety pattern checks..."
	@python3 scripts/check_threading_patterns.py --strict --fix-suggestions

# Full threading safety analysis with report generation
threading-analysis:
	@echo "Running comprehensive threading safety analysis..."
	@python3 scripts/analyze_threading_safety.py src include
	@python3 scripts/check_threading_patterns.py --fix-suggestions --output threading_pattern_issues.txt
	@echo "Analysis complete. Reports generated:"
	@echo "  - threading_safety_analysis.md (static analysis)"
	@echo "  - threading_pattern_issues.txt (pattern violations)"

# Clean threading analysis artifacts
threading-clean:
	@echo "Cleaning threading analysis artifacts..."
	@rm -f threading_safety_analysis.md
	@rm -f threading_pattern_issues.txt
	@rm -f threading_safety_comprehensive_report.md
	@rm -f tests/test_threading_safety_baseline

# Integration with main build targets
# Add threading-check as a dependency to your main build targets
# Example:
# all: threading-check $(MAIN_TARGETS)

# Pre-commit hook integration
# Add this to your pre-commit checks:
pre-commit-threading:
	@echo "Pre-commit threading safety check..."
	@python3 scripts/check_threading_patterns.py --strict

# Continuous integration targets
ci-threading-check: threading-check-strict
	@echo "CI threading safety check passed"

# Development helper targets
threading-help:
	@echo "Threading Safety Makefile Targets:"
	@echo "  threading-check         - Basic pattern checks (errors only)"
	@echo "  threading-check-strict  - Strict checks (warnings as errors)"
	@echo "  threading-analysis      - Full analysis with reports"
	@echo "  threading-clean         - Clean analysis artifacts"
	@echo "  pre-commit-threading    - Pre-commit hook check"
	@echo "  ci-threading-check      - CI pipeline check"
	@echo ""
	@echo "Integration examples:"
	@echo "  make all threading-check     # Check after build"
	@echo "  make threading-analysis      # Generate reports"
	@echo "  make clean threading-clean   # Clean everything"

# File-specific threading checks
# Usage: make threading-check-file FILE=src/audio.cpp
threading-check-file:
	@if [ -z "$(FILE)" ]; then \
		echo "Usage: make threading-check-file FILE=path/to/file.cpp"; \
		exit 1; \
	fi
	@echo "Checking threading patterns in $(FILE)..."
	@python3 scripts/check_threading_patterns.py --fix-suggestions "$(FILE)"

# Directory-specific threading checks
# Usage: make threading-check-dir DIR=src
threading-check-dir:
	@if [ -z "$(DIR)" ]; then \
		echo "Usage: make threading-check-dir DIR=directory"; \
		exit 1; \
	fi
	@echo "Checking threading patterns in $(DIR)/..."
	@find "$(DIR)" -name "*.cpp" -o -name "*.h" | xargs python3 scripts/check_threading_patterns.py --fix-suggestions