#!/bin/bash
#
# Integration script for threading safety checks in PsyMP3 build system
#
# This script integrates threading safety analysis into the existing build system
# by modifying Makefiles and providing build targets for threading checks.
#
# Requirements addressed: 2.3, 5.3

set -e

echo "PsyMP3 Threading Safety Build Integration"
echo "========================================"
echo

# Check if we're in the right directory
if [ ! -f "Makefile" ] && [ ! -f "Makefile.am" ]; then
    echo "Error: Must be run from the PsyMP3 root directory (no Makefile found)"
    exit 1
fi

# Check if threading safety tools exist
if [ ! -f "scripts/check_threading_patterns.py" ]; then
    echo "Error: Threading safety tools not found. Run this script after creating the analysis tools."
    exit 1
fi

echo "Step 1: Making scripts executable..."
chmod +x scripts/analyze_threading_safety.py
chmod +x scripts/check_threading_patterns.py
chmod +x scripts/run_threading_safety_analysis.sh
echo "✓ Scripts are now executable"

echo
echo "Step 2: Integrating with main Makefile..."

# Check if main Makefile exists and add threading targets
if [ -f "Makefile" ]; then
    # Check if threading targets are already integrated
    if grep -q "threading-check" Makefile; then
        echo "✓ Threading targets already integrated in Makefile"
    else
        echo "Adding threading safety targets to Makefile..."
        cat >> Makefile << 'EOF'

# Threading Safety Targets (auto-generated)
include scripts/threading-safety-makefile.mk

EOF
        echo "✓ Threading targets added to Makefile"
    fi
else
    echo "ℹ️  No main Makefile found - you'll need to manually include scripts/threading-safety-makefile.mk"
fi

echo
echo "Step 3: Integrating with tests Makefile..."

# Add threading checks to tests Makefile if it exists
if [ -f "tests/Makefile.am" ]; then
    if grep -q "threading.*check" tests/Makefile.am; then
        echo "✓ Threading checks already integrated in tests/Makefile.am"
    else
        echo "Adding threading safety checks to tests/Makefile.am..."
        
        # Add threading safety test targets
        cat >> tests/Makefile.am << 'EOF'

# Threading Safety Tests (auto-generated)
check-threading:
	cd .. && python3 scripts/check_threading_patterns.py --strict

check-threading-analysis:
	cd .. && ./scripts/run_threading_safety_analysis.sh

.PHONY: check-threading check-threading-analysis

EOF
        echo "✓ Threading checks added to tests/Makefile.am"
    fi
else
    echo "ℹ️  No tests/Makefile.am found - threading tests will be available via main Makefile only"
fi

echo
echo "Step 4: Creating pre-commit hook..."

# Create pre-commit hook if .git directory exists
if [ -d ".git" ]; then
    HOOK_FILE=".git/hooks/pre-commit"
    
    if [ -f "$HOOK_FILE" ]; then
        # Check if threading check is already in the hook
        if grep -q "threading" "$HOOK_FILE"; then
            echo "✓ Threading safety check already in pre-commit hook"
        else
            echo "Adding threading safety check to existing pre-commit hook..."
            # Backup existing hook
            cp "$HOOK_FILE" "$HOOK_FILE.backup"
            
            # Add threading check before the final exit
            sed -i '/^exit/i\
# Threading safety check\
echo "Running threading safety check..."\
python3 scripts/check_threading_patterns.py --strict\
if [ $? -ne 0 ]; then\
    echo "Threading safety check failed. Commit aborted."\
    exit 1\
fi\
' "$HOOK_FILE"
            
            echo "✓ Threading check added to pre-commit hook (backup saved as pre-commit.backup)"
        fi
    else
        echo "Creating new pre-commit hook with threading safety check..."
        cat > "$HOOK_FILE" << 'EOF'
#!/bin/bash
#
# Pre-commit hook with threading safety check
#

echo "Running threading safety check..."
python3 scripts/check_threading_patterns.py --strict

if [ $? -ne 0 ]; then
    echo "Threading safety check failed. Commit aborted."
    echo "Fix the threading issues or use 'git commit --no-verify' to bypass (not recommended)"
    exit 1
fi

echo "Threading safety check passed."
exit 0
EOF
        chmod +x "$HOOK_FILE"
        echo "✓ Pre-commit hook created with threading safety check"
    fi
else
    echo "ℹ️  No .git directory found - skipping pre-commit hook creation"
fi

echo
echo "Step 5: Testing integration..."

# Test that the tools work
echo "Testing threading safety analysis tools..."

if python3 scripts/check_threading_patterns.py --help > /dev/null 2>&1; then
    echo "✓ Pattern checker is working"
else
    echo "✗ Pattern checker test failed"
    exit 1
fi

if python3 scripts/analyze_threading_safety.py --help > /dev/null 2>&1; then
    echo "✓ Static analyzer is working"
else
    echo "✗ Static analyzer test failed"
    exit 1
fi

# Test Makefile integration if possible
if [ -f "Makefile" ] && command -v make > /dev/null 2>&1; then
    if make threading-help > /dev/null 2>&1; then
        echo "✓ Makefile integration is working"
    else
        echo "⚠️  Makefile integration test failed - you may need to run 'make' first"
    fi
fi

echo
echo "Integration Complete!"
echo "==================="
echo
echo "Available commands:"
echo "  make threading-check         - Basic threading safety check"
echo "  make threading-check-strict  - Strict check (warnings as errors)"
echo "  make threading-analysis      - Full analysis with reports"
echo "  make threading-help          - Show all threading targets"
echo
echo "Direct script usage:"
echo "  python3 scripts/check_threading_patterns.py [options] [files]"
echo "  python3 scripts/analyze_threading_safety.py [directories]"
echo "  ./scripts/run_threading_safety_analysis.sh"
echo
echo "Integration features:"
echo "  ✓ Makefile targets for threading safety checks"
echo "  ✓ Pre-commit hook for automatic checking"
echo "  ✓ Build system integration via threading-safety-makefile.mk"
echo "  ✓ Executable scripts with proper permissions"
echo
echo "Next steps:"
echo "  1. Run 'make threading-analysis' to generate initial reports"
echo "  2. Review threading_safety_analysis.md for current issues"
echo "  3. Fix any critical threading safety issues found"
echo "  4. Add 'make threading-check' to your regular build process"
echo
echo "For detailed usage instructions, see:"
echo "  docs/threading-safety-analysis-tools.md"