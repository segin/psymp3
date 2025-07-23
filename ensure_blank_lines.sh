#!/bin/bash

# Script to ensure all .cpp and .h files end with a blank line
# Usage: ./ensure_blank_lines.sh

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
files_processed=0
files_modified=0

echo "Checking for blank lines at end of .cpp and .h files..."

# Function to check and fix a file
fix_file() {
    local file="$1"
    
    # Skip empty files
    if [[ ! -s "$file" ]]; then
        return
    fi
    
    # Check if file ends with a newline
    if [[ $(tail -c1 "$file" | wc -l) -eq 0 ]]; then
        echo -e "${YELLOW}Fixing: $file${NC}"
        echo "" >> "$file"
        ((files_modified++))
    fi
    
    ((files_processed++))
}

# Find and process all .cpp and .h files
while IFS= read -r -d '' file; do
    fix_file "$file"
done < <(find . -type f \( -name "*.cpp" -o -name "*.h" \) -print0)

echo
echo -e "${GREEN}Summary:${NC}"
echo "Files processed: $files_processed"
echo "Files modified: $files_modified"

if [[ $files_modified -gt 0 ]]; then
    echo -e "${YELLOW}$files_modified files were modified to ensure they end with a blank line.${NC}"
else
    echo -e "${GREEN}All files already end with a blank line!${NC}"
fi