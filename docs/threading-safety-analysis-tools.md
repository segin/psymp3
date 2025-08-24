# Threading Safety Analysis Tools

This document describes the static analysis tools and build system integration for maintaining threading safety in the PsyMP3 codebase.

## Overview

The threading safety analysis framework provides automated detection of threading anti-patterns and enforcement of the public/private lock pattern. It consists of several tools that can be integrated into the development workflow and build system.

## Tools Overview

### 1. Static Code Analysis (`scripts/analyze_threading_safety.py`)

**Purpose**: Comprehensive analysis of threading patterns in the codebase.

**Features**:
- Identifies classes that use mutex/lock primitives
- Detects public methods that acquire locks
- Finds potential deadlock scenarios
- Generates detailed analysis reports

**Usage**:
```bash
# Analyze default directories (src/ and include/)
python3 scripts/analyze_threading_safety.py

# Analyze specific directories
python3 scripts/analyze_threading_safety.py src include tests

# Output is written to threading_safety_analysis.md
```

**Output**: Generates `threading_safety_analysis.md` with detailed findings.

### 2. Pattern Checker (`scripts/check_threading_patterns.py`)

**Purpose**: Build system integration for detecting threading anti-patterns.

**Features**:
- Detects manual lock/unlock patterns (should use RAII)
- Verifies public/private lock pattern compliance
- Checks for proper lock ordering documentation
- Identifies callback safety issues
- Validates const method locking patterns

**Usage**:
```bash
# Basic check (errors only)
python3 scripts/check_threading_patterns.py

# Strict mode (warnings as errors)
python3 scripts/check_threading_patterns.py --strict

# With fix suggestions
python3 scripts/check_threading_patterns.py --fix-suggestions

# Check specific files
python3 scripts/check_threading_patterns.py src/audio.cpp include/audio.h

# Output to file
python3 scripts/check_threading_patterns.py --output threading_issues.txt
```

**Exit Codes**:
- `0`: No issues found
- `1`: Warnings found (in strict mode)
- `2`: Errors found

### 3. Comprehensive Analysis (`scripts/run_threading_safety_analysis.sh`)

**Purpose**: Complete analysis workflow including testing and reporting.

**Features**:
- Runs static analysis
- Compiles and executes baseline tests
- Generates comprehensive reports
- Provides actionable recommendations

**Usage**:
```bash
# Run complete analysis
./scripts/run_threading_safety_analysis.sh
```

**Output**: 
- `threading_safety_analysis.md` (static analysis)
- `threading_safety_comprehensive_report.md` (full report)
- Compiled baseline tests

## Build System Integration

### Makefile Integration

Include the threading safety Makefile in your main Makefile:

```makefile
# In your main Makefile
include scripts/threading-safety-makefile.mk

# Add threading check to your build targets
all: threading-check $(YOUR_TARGETS)

# Or run separately
check: threading-check-strict
```

### Available Make Targets

```bash
# Basic pattern checks (errors only)
make threading-check

# Strict checks (warnings as errors)
make threading-check-strict

# Full analysis with reports
make threading-analysis

# Clean analysis artifacts
make threading-clean

# Check specific file
make threading-check-file FILE=src/audio.cpp

# Check specific directory
make threading-check-dir DIR=src

# Pre-commit hook
make pre-commit-threading

# CI pipeline check
make ci-threading-check
```

### Continuous Integration Integration

Add to your CI pipeline:

```yaml
# Example GitHub Actions integration
- name: Threading Safety Check
  run: make ci-threading-check

# Example Jenkins integration
stage('Threading Safety') {
    steps {
        sh 'make threading-check-strict'
    }
}
```

### Pre-commit Hook Integration

Add to `.git/hooks/pre-commit`:

```bash
#!/bin/bash
# Threading safety pre-commit check
make pre-commit-threading
if [ $? -ne 0 ]; then
    echo "Threading safety check failed. Commit aborted."
    exit 1
fi
```

## Rule Reference

### Error Rules (Must Fix)

#### MANUAL_LOCK_UNLOCK
**Description**: Manual lock/unlock detected instead of RAII.
**Fix**: Replace with `std::lock_guard<std::mutex> lock(mutex_name);`

#### MISSING_UNLOCKED_METHOD
**Description**: Public method acquires locks but has no private `_unlocked` counterpart.
**Fix**: Add private method with `_unlocked` suffix that performs work without acquiring locks.

#### CALLBACK_UNDER_LOCK
**Description**: Callback invocation while holding lock.
**Fix**: Release locks before invoking callbacks or queue callbacks for later execution.

### Warning Rules (Should Fix)

#### PUBLIC_CALLS_PUBLIC
**Description**: Public method may call another public method (potential deadlock).
**Fix**: Use `method_unlocked()` version when calling from within the same class.

#### MISSING_LOCK_ORDER_DOC
**Description**: Multiple locks detected but no acquisition order documented.
**Fix**: Add comment documenting lock acquisition order.

#### CONST_METHOD_LOCK
**Description**: Const method acquires lock but mutex may not be mutable.
**Fix**: Declare mutex as `mutable std::mutex mutex_name;`

## Customization and Extension

### Adding New Rules

To add new threading safety rules:

1. **Extend Pattern Checker**: Add new check methods to `ThreadingPatternChecker` class
2. **Define Patterns**: Add regex patterns for detecting the anti-pattern
3. **Add Rule ID**: Create unique rule ID for the new check
4. **Update Documentation**: Document the new rule in this file

Example:
```python
def check_new_pattern(self, file_path: str, lines: List[str]) -> None:
    """Check for new threading anti-pattern"""
    for i, line in enumerate(lines):
        if re.search(r'your_pattern_here', line):
            self.add_issue(
                Severity.ERROR, file_path, i + 1,
                "Description of the issue",
                "Suggested fix",
                "NEW_RULE_ID"
            )
```

### Configuring Severity Levels

Modify the severity levels in `check_threading_patterns.py`:

```python
# Change severity for specific rules
if rule_id == "PUBLIC_CALLS_PUBLIC":
    severity = Severity.ERROR if self.strict_mode else Severity.WARNING
```

### Excluding Files or Directories

Add exclusion patterns to the analysis scripts:

```python
# In analyze_threading_safety.py or check_threading_patterns.py
excluded_patterns = [
    r'tests/.*',  # Exclude test files
    r'.*_generated\.cpp',  # Exclude generated files
]

def should_analyze_file(self, file_path: str) -> bool:
    for pattern in excluded_patterns:
        if re.match(pattern, file_path):
            return False
    return True
```

## Performance Considerations

### Analysis Performance

- **Large Codebases**: The analysis tools use regex-based parsing, which is fast but may be less accurate than full AST parsing
- **Incremental Analysis**: For large projects, consider running analysis only on changed files in CI
- **Parallel Processing**: The tools can be extended to process files in parallel for better performance

### Build Integration Performance

- **Selective Checking**: Use file-specific or directory-specific targets for faster feedback during development
- **Caching**: Consider caching analysis results based on file modification times
- **CI Optimization**: Run full analysis only on main branch, quick checks on feature branches

## Troubleshooting

### Common Issues

#### False Positives
**Problem**: Tool reports issues that aren't actually problems.
**Solution**: 
- Review the specific rule and adjust patterns if needed
- Add exclusions for specific cases
- Consider if the code should be refactored to follow standard patterns

#### Missing Issues
**Problem**: Tool doesn't detect known threading problems.
**Solution**:
- Check if the pattern is covered by existing rules
- Add new rules for the specific pattern
- Verify that the regex patterns are correct

#### Performance Issues
**Problem**: Analysis takes too long.
**Solution**:
- Use incremental analysis for large codebases
- Exclude unnecessary files (tests, generated code)
- Consider using more efficient parsing if needed

### Debugging Analysis Tools

Enable debug output:
```bash
# Add debug flag to scripts
python3 scripts/check_threading_patterns.py --debug

# Or set environment variable
DEBUG=1 python3 scripts/analyze_threading_safety.py
```

## Maintenance

### Regular Updates

1. **Review Rules**: Periodically review and update detection rules based on new patterns found in code reviews
2. **Update Patterns**: Adjust regex patterns as coding styles evolve
3. **Performance Tuning**: Monitor analysis performance and optimize as codebase grows
4. **Documentation**: Keep this documentation updated with new rules and usage patterns

### Integration with Code Reviews

1. **Automated Checks**: Ensure CI runs threading safety checks on all pull requests
2. **Manual Review**: Include threading safety in code review checklists
3. **Training**: Educate team members on threading safety patterns and tool usage
4. **Feedback Loop**: Use findings from manual reviews to improve automated detection

## Future Enhancements

### Planned Improvements

1. **AST-based Analysis**: Replace regex parsing with proper C++ AST parsing for better accuracy
2. **IDE Integration**: Create plugins for popular IDEs to show threading issues in real-time
3. **Automatic Fixes**: Generate automatic fixes for common threading anti-patterns
4. **Performance Profiling**: Add performance impact analysis for threading changes
5. **Cross-platform Support**: Ensure tools work correctly on all supported platforms

### Contributing

To contribute improvements to the threading safety analysis tools:

1. **Follow Patterns**: Use the existing code structure and patterns
2. **Add Tests**: Include tests for new rules and functionality
3. **Update Documentation**: Update this document with new features
4. **Performance**: Ensure new features don't significantly impact analysis performance
5. **Backward Compatibility**: Maintain compatibility with existing usage patterns