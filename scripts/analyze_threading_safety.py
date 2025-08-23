#!/usr/bin/env python3
"""
Threading Safety Analysis Script for PsyMP3

This script analyzes the codebase to identify:
1. Classes that use mutex/lock primitives
2. Public methods that acquire locks
3. Potential deadlock scenarios
4. Missing private unlocked method counterparts

Requirements addressed: 1.1, 1.3, 5.1
"""

import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple, Optional
from dataclasses import dataclass

@dataclass
class LockUsage:
    """Represents lock usage in a method"""
    method_name: str
    is_public: bool
    lock_types: List[str]
    line_number: int
    file_path: str

@dataclass
class ClassAnalysis:
    """Analysis results for a single class"""
    class_name: str
    file_path: str
    mutex_members: List[str]
    public_lock_methods: List[LockUsage]
    private_unlocked_methods: List[str]
    potential_deadlocks: List[str]

class ThreadingSafetyAnalyzer:
    """Analyzes C++ code for threading safety patterns"""
    
    def __init__(self, source_dirs: List[str]):
        self.source_dirs = source_dirs
        self.classes: Dict[str, ClassAnalysis] = {}
        
        # Patterns for detecting locks and mutexes
        self.mutex_patterns = [
            r'std::mutex\s+(\w+)',
            r'std::shared_mutex\s+(\w+)',
            r'std::recursive_mutex\s+(\w+)',
            r'pthread_mutex_t\s+(\w+)',
            r'SDL_mutex\s*\*\s*(\w+)',
        ]
        
        # Patterns for lock acquisition
        self.lock_patterns = [
            r'(\w+)\.lock\(\)',
            r'std::lock_guard<[^>]+>\s+\w+\((\w+)\)',
            r'std::unique_lock<[^>]+>\s+\w+\((\w+)\)',
            r'std::shared_lock<[^>]+>\s+\w+\((\w+)\)',
            r'pthread_mutex_lock\(&(\w+)\)',
            r'SDL_LockMutex\((\w+)\)',
            r'SDL_LockSurface\(',
        ]
    
    def analyze_file(self, file_path: str) -> None:
        """Analyze a single C++ file for threading patterns"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception as e:
            print(f"Warning: Could not read {file_path}: {e}")
            return
        
        # Find class definitions
        class_matches = re.finditer(r'class\s+(\w+)(?:\s*:\s*[^{]+)?\s*{', content)
        
        for class_match in class_matches:
            class_name = class_match.group(1)
            class_start = class_match.start()
            
            # Find the end of the class (simplified - assumes proper bracing)
            brace_count = 0
            class_end = class_start
            in_class = False
            
            for i, char in enumerate(content[class_start:], class_start):
                if char == '{':
                    brace_count += 1
                    in_class = True
                elif char == '}':
                    brace_count -= 1
                    if in_class and brace_count == 0:
                        class_end = i
                        break
            
            if class_end > class_start:
                class_content = content[class_start:class_end + 1]
                self.analyze_class(class_name, file_path, class_content, class_start)
    
    def analyze_class(self, class_name: str, file_path: str, class_content: str, offset: int) -> None:
        """Analyze a single class for threading patterns"""
        analysis = ClassAnalysis(
            class_name=class_name,
            file_path=file_path,
            mutex_members=[],
            public_lock_methods=[],
            private_unlocked_methods=[],
            potential_deadlocks=[]
        )
        
        # Find mutex member variables
        for pattern in self.mutex_patterns:
            matches = re.finditer(pattern, class_content)
            for match in matches:
                analysis.mutex_members.append(match.group(1))
        
        # Find methods and their access levels
        current_access = 'private'  # Default in C++ classes
        lines = class_content.split('\n')
        
        for i, line in enumerate(lines):
            line_stripped = line.strip()
            
            # Track access level changes
            if line_stripped.startswith('public:'):
                current_access = 'public'
                continue
            elif line_stripped.startswith('private:'):
                current_access = 'private'
                continue
            elif line_stripped.startswith('protected:'):
                current_access = 'protected'
                continue
            
            # Look for method definitions
            method_match = re.search(r'(\w+)\s*\([^)]*\)\s*(?:const)?\s*[{;]', line_stripped)
            if method_match:
                method_name = method_match.group(1)
                
                # Skip constructors, destructors, and operators
                if (method_name == class_name or 
                    method_name.startswith('~') or 
                    method_name == 'operator'):
                    continue
                
                # Check if this method acquires locks
                lock_types = self.find_locks_in_method(class_content, method_name)
                if lock_types:
                    lock_usage = LockUsage(
                        method_name=method_name,
                        is_public=(current_access == 'public'),
                        lock_types=lock_types,
                        line_number=i + 1,
                        file_path=file_path
                    )
                    
                    if current_access == 'public':
                        analysis.public_lock_methods.append(lock_usage)
                
                # Check for private unlocked methods
                if current_access == 'private' and method_name.endswith('_unlocked'):
                    analysis.private_unlocked_methods.append(method_name)
        
        # Analyze potential deadlocks
        analysis.potential_deadlocks = self.find_potential_deadlocks(analysis)
        
        # Only store classes that have mutex usage
        if analysis.mutex_members or analysis.public_lock_methods:
            self.classes[class_name] = analysis
    
    def find_locks_in_method(self, class_content: str, method_name: str) -> List[str]:
        """Find lock acquisitions within a specific method"""
        # This is a simplified implementation - in practice, you'd need
        # more sophisticated parsing to handle method boundaries correctly
        lock_types = []
        
        for pattern in self.lock_patterns:
            if re.search(pattern, class_content):
                lock_types.append(pattern)
        
        return lock_types
    
    def find_potential_deadlocks(self, analysis: ClassAnalysis) -> List[str]:
        """Identify potential deadlock scenarios"""
        issues = []
        
        # Check for public methods that acquire locks without private counterparts
        for lock_method in analysis.public_lock_methods:
            expected_private = f"{lock_method.method_name}_unlocked"
            if expected_private not in analysis.private_unlocked_methods:
                issues.append(
                    f"Public method '{lock_method.method_name}' acquires locks but has no "
                    f"private '{expected_private}' counterpart"
                )
        
        # Check for multiple mutex members (potential lock ordering issues)
        if len(analysis.mutex_members) > 1:
            issues.append(
                f"Class has multiple mutexes ({', '.join(analysis.mutex_members)}) - "
                f"verify lock acquisition order"
            )
        
        return issues
    
    def analyze_all(self) -> None:
        """Analyze all C++ files in the specified directories"""
        for source_dir in self.source_dirs:
            if not os.path.exists(source_dir):
                print(f"Warning: Directory {source_dir} does not exist")
                continue
            
            for root, dirs, files in os.walk(source_dir):
                for file in files:
                    if file.endswith(('.cpp', '.h')):
                        file_path = os.path.join(root, file)
                        self.analyze_file(file_path)
    
    def generate_report(self) -> str:
        """Generate a comprehensive threading safety report"""
        report = []
        report.append("# Threading Safety Analysis Report")
        report.append("=" * 50)
        report.append("")
        
        if not self.classes:
            report.append("No classes with mutex usage found.")
            return "\n".join(report)
        
        report.append(f"Found {len(self.classes)} classes with threading concerns:")
        report.append("")
        
        for class_name, analysis in sorted(self.classes.items()):
            report.append(f"## Class: {class_name}")
            report.append(f"File: {analysis.file_path}")
            report.append("")
            
            if analysis.mutex_members:
                report.append("### Mutex Members:")
                for mutex in analysis.mutex_members:
                    report.append(f"  - {mutex}")
                report.append("")
            
            if analysis.public_lock_methods:
                report.append("### Public Lock-Acquiring Methods:")
                for method in analysis.public_lock_methods:
                    report.append(f"  - {method.method_name} (line {method.line_number})")
                report.append("")
            
            if analysis.private_unlocked_methods:
                report.append("### Private Unlocked Methods:")
                for method in analysis.private_unlocked_methods:
                    report.append(f"  - {method}")
                report.append("")
            
            if analysis.potential_deadlocks:
                report.append("### ⚠️  Potential Issues:")
                for issue in analysis.potential_deadlocks:
                    report.append(f"  - {issue}")
                report.append("")
            
            report.append("-" * 40)
            report.append("")
        
        # Summary statistics
        total_public_lock_methods = sum(len(a.public_lock_methods) for a in self.classes.values())
        total_issues = sum(len(a.potential_deadlocks) for a in self.classes.values())
        
        report.append("## Summary")
        report.append(f"- Classes analyzed: {len(self.classes)}")
        report.append(f"- Public lock-acquiring methods: {total_public_lock_methods}")
        report.append(f"- Potential threading issues: {total_issues}")
        
        return "\n".join(report)

def main():
    """Main entry point for the threading safety analyzer"""
    if len(sys.argv) > 1:
        source_dirs = sys.argv[1:]
    else:
        # Default directories for PsyMP3
        source_dirs = ['src', 'include']
    
    print("Threading Safety Analysis for PsyMP3")
    print("=" * 40)
    
    analyzer = ThreadingSafetyAnalyzer(source_dirs)
    analyzer.analyze_all()
    
    report = analyzer.generate_report()
    
    # Write report to file
    report_file = 'threading_safety_analysis.md'
    with open(report_file, 'w') as f:
        f.write(report)
    
    print(f"Analysis complete. Report written to {report_file}")
    print("\nQuick Summary:")
    print(f"- Found {len(analyzer.classes)} classes with threading concerns")
    
    # Print classes with the most issues
    if analyzer.classes:
        issues_by_class = [(name, len(analysis.potential_deadlocks)) 
                          for name, analysis in analyzer.classes.items()]
        issues_by_class.sort(key=lambda x: x[1], reverse=True)
        
        print("\nClasses with most potential issues:")
        for class_name, issue_count in issues_by_class[:5]:
            if issue_count > 0:
                print(f"  - {class_name}: {issue_count} issues")

if __name__ == '__main__':
    main()