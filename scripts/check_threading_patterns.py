#!/usr/bin/env python3
"""
Enhanced Threading Pattern Checker for PsyMP3

This script provides build system integration for detecting threading anti-patterns
and enforcing the public/private lock pattern.

Usage:
  python3 scripts/check_threading_patterns.py [--strict] [--fix-suggestions] [files...]

Requirements addressed: 2.3, 5.1, 5.3
"""

import os
import re
import sys
import argparse
from pathlib import Path
from typing import Dict, List, Set, Tuple, Optional
from dataclasses import dataclass
from enum import Enum

class Severity(Enum):
    ERROR = "ERROR"
    WARNING = "WARNING"
    INFO = "INFO"

@dataclass
class ThreadingIssue:
    """Represents a threading safety issue"""
    severity: Severity
    file_path: str
    line_number: int
    message: str
    suggestion: Optional[str] = None
    rule_id: str = ""

class ThreadingPatternChecker:
    """Enhanced checker for threading safety patterns"""
    
    def __init__(self, strict_mode: bool = False, suggest_fixes: bool = False):
        self.strict_mode = strict_mode
        self.suggest_fixes = suggest_fixes
        self.issues: List[ThreadingIssue] = []
        
        # Patterns for detecting various threading constructs
        self.mutex_declaration_patterns = [
            r'std::(mutex|shared_mutex|recursive_mutex)\s+(\w+)',
            r'pthread_mutex_t\s+(\w+)',
            r'SDL_mutex\s*\*\s*(\w+)',
        ]
        
        self.lock_acquisition_patterns = [
            (r'(\w+)\.lock\(\)', 'mutex.lock()'),
            (r'std::lock_guard<[^>]+>\s+\w+\(([^)]+)\)', 'lock_guard'),
            (r'std::unique_lock<[^>]+>\s+\w+\(([^)]+)\)', 'unique_lock'),
            (r'std::shared_lock<[^>]+>\s+\w+\(([^)]+)\)', 'shared_lock'),
            (r'pthread_mutex_lock\(&(\w+)\)', 'pthread_mutex_lock'),
            (r'SDL_LockMutex\((\w+)\)', 'SDL_LockMutex'),
            (r'SDL_LockSurface\([^)]+\)', 'SDL_LockSurface'),
        ]
        
        self.manual_unlock_patterns = [
            r'(\w+)\.unlock\(\)',
            r'pthread_mutex_unlock\(&(\w+)\)',
            r'SDL_UnlockMutex\((\w+)\)',
            r'SDL_UnlockSurface\([^)]+\)',
        ]
    
    def check_file(self, file_path: str) -> None:
        """Check a single file for threading pattern violations"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception as e:
            self.add_issue(Severity.WARNING, file_path, 0, 
                          f"Could not read file: {e}", rule_id="FILE_READ_ERROR")
            return
        
        lines = content.split('\n')
        
        # Check for various threading anti-patterns
        self.check_manual_lock_unlock(file_path, lines)
        self.check_public_private_pattern(file_path, content, lines)
        self.check_lock_ordering(file_path, lines)
        self.check_callback_safety(file_path, lines)
        self.check_const_method_locking(file_path, lines)
    
    def check_manual_lock_unlock(self, file_path: str, lines: List[str]) -> None:
        """Check for manual lock/unlock patterns (should use RAII)"""
        for i, line in enumerate(lines):
            line_stripped = line.strip()
            
            # Check for manual unlock calls
            for pattern in self.manual_unlock_patterns:
                if re.search(pattern, line_stripped):
                    # Look for corresponding lock in nearby lines
                    context_start = max(0, i - 10)
                    context_end = min(len(lines), i + 10)
                    context = '\n'.join(lines[context_start:context_end])
                    
                    # Check if this is part of a RAII pattern
                    if not re.search(r'std::(lock_guard|unique_lock|shared_lock)', context):
                        self.add_issue(
                            Severity.ERROR, file_path, i + 1,
                            "Manual lock/unlock detected - use RAII lock guards instead",
                            "Replace with std::lock_guard<std::mutex> lock(mutex_name);",
                            "MANUAL_LOCK_UNLOCK"
                        )
    
    def check_public_private_pattern(self, file_path: str, content: str, lines: List[str]) -> None:
        """Check for proper public/private lock pattern implementation"""
        # Find class definitions
        class_matches = list(re.finditer(r'class\s+(\w+)(?:\s*:\s*[^{]+)?\s*{', content))
        
        for class_match in class_matches:
            class_name = class_match.group(1)
            class_start = class_match.start()
            
            # Find class boundaries (simplified)
            class_content = self.extract_class_content(content, class_start)
            if not class_content:
                continue
            
            self.analyze_class_threading_pattern(file_path, class_name, class_content)
    
    def extract_class_content(self, content: str, start_pos: int) -> Optional[str]:
        """Extract the content of a class definition"""
        brace_count = 0
        class_end = start_pos
        in_class = False
        
        for i, char in enumerate(content[start_pos:], start_pos):
            if char == '{':
                brace_count += 1
                in_class = True
            elif char == '}':
                brace_count -= 1
                if in_class and brace_count == 0:
                    class_end = i
                    break
        
        if class_end > start_pos:
            return content[start_pos:class_end + 1]
        return None
    
    def analyze_class_threading_pattern(self, file_path: str, class_name: str, class_content: str) -> None:
        """Analyze a class for proper threading patterns"""
        # Check if class uses mutexes
        has_mutex = any(re.search(pattern, class_content) for pattern, _ in self.mutex_declaration_patterns)
        if not has_mutex:
            return  # No threading concerns
        
        # Find public methods that acquire locks
        public_lock_methods = self.find_public_lock_methods(class_content)
        
        # Find private unlocked methods
        private_unlocked_methods = self.find_private_unlocked_methods(class_content)
        
        # Check for missing private counterparts
        for method_name, line_num in public_lock_methods:
            expected_private = f"{method_name}_unlocked"
            if expected_private not in private_unlocked_methods:
                self.add_issue(
                    Severity.ERROR, file_path, line_num,
                    f"Public method '{method_name}' acquires locks but has no private '{expected_private}' counterpart",
                    f"Add private method: {method_name}_unlocked() that performs the work without acquiring locks",
                    "MISSING_UNLOCKED_METHOD"
                )
        
        # Check for public methods calling other public methods
        self.check_public_method_calls(file_path, class_content, public_lock_methods)
    
    def find_public_lock_methods(self, class_content: str) -> List[Tuple[str, int]]:
        """Find public methods that acquire locks"""
        methods = []
        lines = class_content.split('\n')
        current_access = 'private'  # Default in C++ classes
        
        for i, line in enumerate(lines):
            line_stripped = line.strip()
            
            # Track access level
            if line_stripped.startswith('public:'):
                current_access = 'public'
                continue
            elif line_stripped.startswith('private:') or line_stripped.startswith('protected:'):
                current_access = 'private'
                continue
            
            # Look for method definitions in public section
            if current_access == 'public':
                method_match = re.search(r'(\w+)\s*\([^)]*\)\s*(?:const)?\s*[{;]', line_stripped)
                if method_match:
                    method_name = method_match.group(1)
                    
                    # Skip constructors, destructors, operators
                    if (method_name.startswith('~') or 
                        method_name == 'operator' or
                        method_name[0].isupper()):  # Likely constructor
                        continue
                    
                    # Check if method body contains lock acquisition
                    if self.method_acquires_locks(class_content, method_name, i):
                        methods.append((method_name, i + 1))
        
        return methods
    
    def find_private_unlocked_methods(self, class_content: str) -> Set[str]:
        """Find private methods with _unlocked suffix"""
        methods = set()
        lines = class_content.split('\n')
        current_access = 'private'
        
        for line in lines:
            line_stripped = line.strip()
            
            # Track access level
            if line_stripped.startswith('public:'):
                current_access = 'public'
                continue
            elif line_stripped.startswith('private:'):
                current_access = 'private'
                continue
            elif line_stripped.startswith('protected:'):
                current_access = 'protected'
                continue
            
            # Look for _unlocked methods in private section
            if current_access == 'private':
                method_match = re.search(r'(\w+_unlocked)\s*\([^)]*\)', line_stripped)
                if method_match:
                    methods.add(method_match.group(1))
        
        return methods
    
    def method_acquires_locks(self, class_content: str, method_name: str, start_line: int) -> bool:
        """Check if a method acquires locks"""
        # This is a simplified check - in practice, you'd need more sophisticated parsing
        lines = class_content.split('\n')
        
        # Look for lock patterns in the method vicinity
        context_start = max(0, start_line - 2)
        context_end = min(len(lines), start_line + 20)  # Assume methods aren't too long
        context = '\n'.join(lines[context_start:context_end])
        
        for pattern, _ in self.lock_acquisition_patterns:
            if re.search(pattern, context):
                return True
        
        return False
    
    def check_public_method_calls(self, file_path: str, class_content: str, public_methods: List[Tuple[str, int]]) -> None:
        """Check for public methods calling other public methods"""
        public_method_names = {name for name, _ in public_methods}
        
        for method_name, line_num in public_methods:
            # Look for calls to other public methods within this method
            # This is a simplified check
            for other_method, _ in public_methods:
                if other_method != method_name:
                    pattern = rf'{other_method}\s*\('
                    if re.search(pattern, class_content):
                        self.add_issue(
                            Severity.WARNING, file_path, line_num,
                            f"Public method '{method_name}' may call public method '{other_method}' - potential deadlock risk",
                            f"Use '{other_method}_unlocked()' instead if calling from within the same class",
                            "PUBLIC_CALLS_PUBLIC"
                        )
    
    def check_lock_ordering(self, file_path: str, lines: List[str]) -> None:
        """Check for consistent lock ordering documentation"""
        has_multiple_locks = False
        has_lock_order_doc = False
        
        for i, line in enumerate(lines):
            # Check for multiple lock acquisitions in same scope
            lock_count = sum(1 for pattern, _ in self.lock_acquisition_patterns 
                           if re.search(pattern, line))
            if lock_count > 1:
                has_multiple_locks = True
            
            # Check for lock order documentation
            if re.search(r'lock.*order|acquisition.*order', line.lower()):
                has_lock_order_doc = True
        
        if has_multiple_locks and not has_lock_order_doc:
            self.add_issue(
                Severity.WARNING, file_path, 1,
                "Multiple locks detected but no lock acquisition order documented",
                "Add comment documenting lock acquisition order to prevent deadlocks",
                "MISSING_LOCK_ORDER_DOC"
            )
    
    def check_callback_safety(self, file_path: str, lines: List[str]) -> None:
        """Check for callback invocations while holding locks"""
        for i, line in enumerate(lines):
            line_stripped = line.strip()
            
            # Look for callback patterns
            callback_patterns = [
                r'(\w+)\s*\(',  # Function call
                r'(\w+)->(\w+)\s*\(',  # Method call via pointer
                r'(\w+)\.(\w+)\s*\(',  # Method call via object
            ]
            
            # Check if line contains both lock and callback
            has_lock = any(re.search(pattern, line_stripped) for pattern, _ in self.lock_acquisition_patterns)
            has_callback = any(re.search(pattern, line_stripped) for pattern in callback_patterns)
            
            if has_lock and 'callback' in line_stripped.lower():
                self.add_issue(
                    Severity.ERROR, file_path, i + 1,
                    "Potential callback invocation while holding lock - deadlock risk",
                    "Release locks before invoking callbacks or queue callbacks for later execution",
                    "CALLBACK_UNDER_LOCK"
                )
    
    def check_const_method_locking(self, file_path: str, lines: List[str]) -> None:
        """Check for const methods that need mutable mutexes"""
        for i, line in enumerate(lines):
            line_stripped = line.strip()
            
            # Look for const methods that acquire locks
            if 'const' in line_stripped and any(re.search(pattern, line_stripped) for pattern, _ in self.lock_acquisition_patterns):
                # Check if mutex is declared as mutable
                context_start = max(0, i - 20)
                context_end = min(len(lines), i + 5)
                context = '\n'.join(lines[context_start:context_end])
                
                if 'mutable' not in context:
                    self.add_issue(
                        Severity.WARNING, file_path, i + 1,
                        "Const method acquires lock - ensure mutex is declared as mutable",
                        "Declare mutex as 'mutable std::mutex mutex_name;'",
                        "CONST_METHOD_LOCK"
                    )
    
    def add_issue(self, severity: Severity, file_path: str, line_number: int, 
                  message: str, suggestion: Optional[str] = None, rule_id: str = "") -> None:
        """Add a threading issue to the list"""
        issue = ThreadingIssue(
            severity=severity,
            file_path=file_path,
            line_number=line_number,
            message=message,
            suggestion=suggestion,
            rule_id=rule_id
        )
        self.issues.append(issue)
    
    def generate_report(self) -> str:
        """Generate a report of all threading issues"""
        if not self.issues:
            return "✓ No threading safety issues detected."
        
        report = []
        report.append("Threading Safety Issues Report")
        report.append("=" * 40)
        report.append("")
        
        # Group issues by severity
        errors = [i for i in self.issues if i.severity == Severity.ERROR]
        warnings = [i for i in self.issues if i.severity == Severity.WARNING]
        infos = [i for i in self.issues if i.severity == Severity.INFO]
        
        if errors:
            report.append("🚨 ERRORS (must fix):")
            for issue in errors:
                report.append(f"  {issue.file_path}:{issue.line_number} - {issue.message}")
                if issue.suggestion and self.suggest_fixes:
                    report.append(f"    💡 Suggestion: {issue.suggestion}")
            report.append("")
        
        if warnings:
            report.append("⚠️  WARNINGS (should fix):")
            for issue in warnings:
                report.append(f"  {issue.file_path}:{issue.line_number} - {issue.message}")
                if issue.suggestion and self.suggest_fixes:
                    report.append(f"    💡 Suggestion: {issue.suggestion}")
            report.append("")
        
        if infos:
            report.append("ℹ️  INFO (consider fixing):")
            for issue in infos:
                report.append(f"  {issue.file_path}:{issue.line_number} - {issue.message}")
                if issue.suggestion and self.suggest_fixes:
                    report.append(f"    💡 Suggestion: {issue.suggestion}")
            report.append("")
        
        report.append(f"Summary: {len(errors)} errors, {len(warnings)} warnings, {len(infos)} info")
        
        return "\n".join(report)
    
    def get_exit_code(self) -> int:
        """Get appropriate exit code based on issues found"""
        errors = [i for i in self.issues if i.severity == Severity.ERROR]
        warnings = [i for i in self.issues if i.severity == Severity.WARNING]
        
        if errors:
            return 2  # Critical errors
        elif warnings and self.strict_mode:
            return 1  # Warnings in strict mode
        else:
            return 0  # Success

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Check threading safety patterns in C++ code')
    parser.add_argument('files', nargs='*', help='Files to check (default: src/ and include/)')
    parser.add_argument('--strict', action='store_true', help='Treat warnings as errors')
    parser.add_argument('--fix-suggestions', action='store_true', help='Include fix suggestions in output')
    parser.add_argument('--output', help='Output file for report')
    
    args = parser.parse_args()
    
    # Determine files to check
    if args.files:
        files_to_check = args.files
    else:
        # Default to all C++ files in src/ and include/
        files_to_check = []
        for directory in ['src', 'include']:
            if os.path.exists(directory):
                for root, dirs, files in os.walk(directory):
                    for file in files:
                        if file.endswith(('.cpp', '.h')):
                            files_to_check.append(os.path.join(root, file))
    
    # Run the checker
    checker = ThreadingPatternChecker(strict_mode=args.strict, suggest_fixes=args.fix_suggestions)
    
    for file_path in files_to_check:
        if os.path.exists(file_path):
            checker.check_file(file_path)
        else:
            print(f"Warning: File {file_path} does not exist", file=sys.stderr)
    
    # Generate and output report
    report = checker.generate_report()
    
    if args.output:
        with open(args.output, 'w') as f:
            f.write(report)
        print(f"Report written to {args.output}")
    else:
        print(report)
    
    # Exit with appropriate code
    sys.exit(checker.get_exit_code())

if __name__ == '__main__':
    main()