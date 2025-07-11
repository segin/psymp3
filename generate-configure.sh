#!/bin/bash
#
# generate-configure.sh - Generate configure script from autotools sources
# This file is part of PsyMP3.
# Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>

set -e  # Exit on any error
set -u  # Exit on undefined variables

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_NAME="PsyMP3"

# Color output support
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
    RED=$(tput setaf 1)
    GREEN=$(tput setaf 2)
    YELLOW=$(tput setaf 3)
    BLUE=$(tput setaf 4)
    BOLD=$(tput bold)
    RESET=$(tput sgr0)
else
    RED="" GREEN="" YELLOW="" BLUE="" BOLD="" RESET=""
fi

# Logging functions
log_info() {
    echo "${BLUE}[INFO]${RESET} $*"
}

log_success() {
    echo "${GREEN}[SUCCESS]${RESET} $*"
}

log_warning() {
    echo "${YELLOW}[WARNING]${RESET} $*"
}

log_error() {
    echo "${RED}[ERROR]${RESET} $*" >&2
}

# Check if we're in the correct directory
check_project_directory() {
    if [ ! -f "configure.ac" ]; then
        log_error "configure.ac not found. Please run this script from the project root directory."
        exit 1
    fi
    
    if [ ! -f "Makefile.am" ]; then
        log_error "Makefile.am not found. Please run this script from the project root directory."
        exit 1
    fi
    
    log_info "Found autotools configuration files in $(pwd)"
}

# Check for required tools
check_autotools() {
    local missing_tools=()
    
    for tool in aclocal automake autoconf autoheader; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            missing_tools+=("$tool")
        fi
    done
    
    if [ ${#missing_tools[@]} -gt 0 ]; then
        log_error "Missing required autotools: ${missing_tools[*]}"
        log_error "Please install the autotools package for your distribution:"
        log_error "  Ubuntu/Debian: sudo apt-get install autotools-dev automake autoconf"
        log_error "  CentOS/RHEL:   sudo yum install autotools automake autoconf"
        log_error "  Fedora:        sudo dnf install autotools automake autoconf"
        log_error "  macOS:         brew install autotools automake autoconf"
        exit 1
    fi
    
    log_info "All required autotools found"
}

# Show tool versions
show_versions() {
    log_info "Autotools versions:"
    for tool in aclocal automake autoconf autoheader; do
        if command -v "$tool" >/dev/null 2>&1; then
            version=$("$tool" --version 2>/dev/null | head -n1 | sed 's/^[^0-9]*\([0-9][0-9.]*\).*$/\1/' || echo "unknown")
            printf "  %-12s %s\n" "$tool:" "$version"
        fi
    done
}

# Clean up old generated files
clean_old_files() {
    log_info "Cleaning up old generated files..."
    
    # Remove autotools cache
    rm -rf autom4te.cache
    
    # Remove old generated files (but preserve user-modified files)
    for file in aclocal.m4 configure config.guess config.sub depcomp install-sh missing compile; do
        if [ -f "$file" ]; then
            rm -f "$file"
        fi
    done
    
    # Remove old Makefile.in files
    find . -name "Makefile.in" -type f -delete 2>/dev/null || true
    
    log_info "Cleanup complete"
}

# Generate configure script
generate_configure() {
    log_info "Generating configure script for $PROJECT_NAME..."
    
    # Use autoreconf for modern, comprehensive regeneration
    log_info "Running autoreconf -fiv..."
    if autoreconf -fiv; then
        log_success "Configure script generated successfully"
    else
        log_error "Failed to generate configure script"
        exit 1
    fi
}

# Verify the generated configure script
verify_configure() {
    if [ ! -f "configure" ]; then
        log_error "Configure script was not generated"
        exit 1
    fi
    
    if [ ! -x "configure" ]; then
        log_warning "Configure script is not executable, making it executable..."
        chmod +x configure
    fi
    
    log_info "Verifying configure script..."
    if ./configure --help >/dev/null 2>&1; then
        log_success "Configure script verification passed"
    else
        log_error "Configure script verification failed"
        exit 1
    fi
}

# Main execution
main() {
    log_info "Starting autotools configuration generation for $PROJECT_NAME"
    
    # Perform checks and setup
    check_project_directory
    check_autotools
    show_versions
    
    # Generate the configure script
    clean_old_files
    generate_configure
    verify_configure
    
    log_success "Configure script generation complete!"
    log_info "You can now run: ./configure && make"
    log_info "For parallel builds, use: make -j8"
}

# Handle command line arguments
case "${1:-}" in
    --help|-h)
        cat << EOF
Usage: $0 [OPTIONS]

Generate configure script from autotools sources for $PROJECT_NAME.

Options:
  --help, -h     Show this help message
  --version, -v  Show version information
  --clean        Clean old files without regenerating
  --check        Check requirements without generating

This script will:
1. Verify autotools installation
2. Clean old generated files
3. Generate new configure script using autoreconf
4. Verify the generated script works

EOF
        exit 0
        ;;
    --version|-v)
        echo "$PROJECT_NAME configure script generator"
        echo "Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>"
        exit 0
        ;;
    --clean)
        check_project_directory
        clean_old_files
        log_success "Cleanup complete"
        exit 0
        ;;
    --check)
        check_project_directory
        check_autotools
        show_versions
        log_success "All requirements satisfied"
        exit 0
        ;;
    --*|-*)
        log_error "Unknown option: $1"
        log_error "Use --help for usage information"
        exit 1
        ;;
    "")
        # No arguments, run main function
        main
        ;;
    *)
        log_error "Unexpected argument: $1"
        log_error "Use --help for usage information"
        exit 1
        ;;
esac