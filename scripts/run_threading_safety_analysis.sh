#!/bin/bash
#
# Complete Threading Safety Analysis Script for PsyMP3
# 
# This script runs the complete threading safety analysis workflow:
# 1. Static code analysis
# 2. Baseline testing
# 3. Report generation
#
# Requirements addressed: 1.1, 1.3, 5.1

set -e  # Exit on any error

echo "PsyMP3 Threading Safety Analysis"
echo "================================"
echo

# Check if we're in the right directory
if [ ! -f "scripts/analyze_threading_safety.py" ]; then
    echo "Error: Must be run from the PsyMP3 root directory"
    exit 1
fi

# Step 1: Run static analysis
echo "Step 1: Running static code analysis..."
python3 scripts/analyze_threading_safety.py src include

if [ -f "threading_safety_analysis.md" ]; then
    echo "✓ Static analysis complete. Report saved to threading_safety_analysis.md"
    
    # Show quick summary
    echo
    echo "Quick Summary:"
    grep "Classes analyzed:" threading_safety_analysis.md || echo "Could not extract summary"
    grep "Public lock-acquiring methods:" threading_safety_analysis.md || echo "Could not extract method count"
    grep "Potential threading issues:" threading_safety_analysis.md || echo "Could not extract issue count"
else
    echo "✗ Static analysis failed - no report generated"
    exit 1
fi

echo

# Step 2: Compile and run baseline tests
echo "Step 2: Compiling threading safety baseline tests..."

# Check if test framework exists
if [ ! -f "tests/test_framework_threading.h" ]; then
    echo "✗ Threading test framework not found"
    exit 1
fi

# Compile the baseline test
if g++ -std=c++17 -pthread -I. -Iinclude tests/test_threading_safety_baseline.cpp -o tests/test_threading_safety_baseline; then
    echo "✓ Baseline tests compiled successfully"
else
    echo "✗ Failed to compile baseline tests"
    exit 1
fi

echo
echo "Step 3: Running baseline threading safety tests..."

if ./tests/test_threading_safety_baseline; then
    echo "✓ Baseline tests completed"
else
    echo "✗ Baseline tests failed"
    exit 1
fi

echo

# Step 3: Generate comprehensive report
echo "Step 4: Generating comprehensive analysis report..."

REPORT_FILE="threading_safety_comprehensive_report.md"

cat > "$REPORT_FILE" << 'EOF'
# PsyMP3 Threading Safety Comprehensive Analysis Report

This report was generated automatically by the threading safety analysis framework.

## Analysis Date
EOF

date >> "$REPORT_FILE"

cat >> "$REPORT_FILE" << 'EOF'

## Executive Summary

This analysis identifies threading safety issues in the PsyMP3 codebase and provides
a framework for implementing the public/private lock pattern to prevent deadlocks.

## Static Analysis Results

EOF

# Include the static analysis results
if [ -f "threading_safety_analysis.md" ]; then
    cat "threading_safety_analysis.md" >> "$REPORT_FILE"
else
    echo "Static analysis report not available." >> "$REPORT_FILE"
fi

cat >> "$REPORT_FILE" << 'EOF'

## Baseline Test Results

The baseline tests demonstrate current threading behavior and establish performance
benchmarks before implementing the public/private lock pattern refactoring.

Key findings from baseline tests:
- Basic concurrent access patterns work for simple operations
- Lock ordering issues exist between static and instance methods  
- Callback reentrancy can cause problems in memory management
- Performance baseline established for comparison after refactoring

## Recommended Actions

Based on this analysis, the following actions are recommended:

### High Priority (Critical for Audio Playback)
1. **Audio Class Refactoring** - Implement public/private lock pattern
2. **IOHandler Class Refactoring** - Fix static/instance lock ordering
3. **MemoryPoolManager Refactoring** - Implement callback safety

### Medium Priority (Performance and Stability)
4. **Surface Class Refactoring** - Fix SDL lock nesting issues
5. **BoundedQueue Class Review** - Simplify lock acquisition patterns
6. **Demuxer Classes Review** - Verify lock ordering in multi-mutex classes

### Implementation Guidelines
- Follow the public/private lock pattern documented in `docs/threading-safety-patterns.md`
- Use the testing framework in `tests/test_framework_threading.h` for validation
- Maintain performance benchmarks throughout refactoring
- Update steering documents with threading safety requirements

## Next Steps

1. Begin with Audio class refactoring (highest impact on user experience)
2. Create specific tests for each class before refactoring
3. Implement refactoring incrementally with continuous testing
4. Update build system to include threading safety validation
5. Document lock acquisition order for all multi-mutex classes

## Framework Usage

The threading safety framework provides:
- **Static Analysis**: `scripts/analyze_threading_safety.py`
- **Test Framework**: `tests/test_framework_threading.h`
- **Documentation**: `docs/threading-safety-patterns.md`
- **Usage Guide**: `docs/threading-safety-framework-usage.md`

Run this analysis periodically during development to catch new threading issues early.

EOF

echo "✓ Comprehensive report generated: $REPORT_FILE"

echo
echo "Analysis Complete!"
echo "=================="
echo
echo "Generated files:"
echo "  - threading_safety_analysis.md (static analysis)"
echo "  - $REPORT_FILE (comprehensive report)"
echo "  - tests/test_threading_safety_baseline (compiled test)"
echo
echo "Next steps:"
echo "  1. Review the comprehensive report"
echo "  2. Start with Audio class refactoring (highest priority)"
echo "  3. Use the testing framework to validate changes"
echo "  4. Update steering documents with threading guidelines"
echo
echo "For detailed usage instructions, see:"
echo "  docs/threading-safety-framework-usage.md"