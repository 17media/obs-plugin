#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Extract locale keys from OBS plugin and generate ini file

This script traverses all C/C++ files in the src directory, searches for obs_module_text() calls,
extracts locale keys from them, and generates an ini file in alphabetical order.
"""

import os
import re
import sys
from pathlib import Path
from typing import Set, List


def find_cpp_files(directory: str) -> List[Path]:
    """
    Recursively find all C/C++ files in the directory
    
    Args:
        directory: Directory path to search
        
    Returns:
        List of C/C++ file paths
    """
    cpp_extensions = {'.c', '.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx'}
    cpp_files = []
    
    for root, dirs, files in os.walk(directory):
        for file in files:
            file_path = Path(root) / file
            if file_path.suffix.lower() in cpp_extensions:
                cpp_files.append(file_path)
    
    return cpp_files


def extract_locale_keys(file_path: Path) -> Set[str]:
    """
    Extract obs_module_text locale keys from a single file
    
    Args:
        file_path: File path
        
    Returns:
        Set of extracted locale keys
    """
    locale_keys = set()
    
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            
        # Match obs_module_text("key") pattern
        # Pattern for double quotes
        pattern_double = r'obs_module_text\s*\(\s*"([^"]+)"\s*\)'
        matches_double = re.findall(pattern_double, content)
        
        # Pattern for single quotes
        pattern_single = r"obs_module_text\s*\(\s*'([^']+)'\s*\)"
        matches_single = re.findall(pattern_single, content)
        
        # Merge all match results
        all_matches = matches_double + matches_single
        
        for match in all_matches:
            # Clean key, remove extra whitespace
            key = match.strip()
            if key:  # Ensure key is not empty
                locale_keys.add(key)
                
    except Exception as e:
        print(f"Warning: Error reading file {file_path}: {e}", file=sys.stderr)
    
    return locale_keys


def generate_ini_content(locale_keys: Set[str]) -> str:
    """
    Generate ini file content
    
    Args:
        locale_keys: Set of locale keys
        
    Returns:
        ini file content string
    """
    if not locale_keys:
        return "# No locale keys found\n"
    
    # Sort alphabetically
    sorted_keys = sorted(locale_keys)
    
    lines = [
        "# OBS 17Live Plugin Locale Keys",
        "# Auto-generated locale key list",
        "# Please provide corresponding translations for each key",
        ""
    ]
    
    for key in sorted_keys:
        # Generate key=value format, value is temporarily empty or uses key as default
        lines.append(f"{key}={key}")
    
    return "\n".join(lines) + "\n"


def main():
    """
    Main function
    """
    # Get parent directory of script directory (project root directory)
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    src_dir = project_root / "src"
    
    if not src_dir.exists():
        print(f"Error: src directory does not exist: {src_dir}", file=sys.stderr)
        sys.exit(1)
    
    print(f"Scanning directory: {src_dir}")
    
    # Find all C/C++ files
    cpp_files = find_cpp_files(str(src_dir))
    print(f"Found {len(cpp_files)} C/C++ files")
    
    # Extract all locale keys
    all_locale_keys = set()
    processed_files = 0
    
    for file_path in cpp_files:
        keys = extract_locale_keys(file_path)
        if keys:
            print(f"Extracted {len(keys)} keys from {file_path.relative_to(project_root)}")
            all_locale_keys.update(keys)
        processed_files += 1
    
    print(f"\nProcessing completed:")
    print(f"- Files processed: {processed_files}")
    print(f"- Unique locale keys extracted: {len(all_locale_keys)}")
    
    if all_locale_keys:
        print("\nFound locale keys:")
        for key in sorted(all_locale_keys):
            print(f"  - {key}")
    
    # Generate ini file
    ini_content = generate_ini_content(all_locale_keys)
    output_file = script_dir / "extracted_locale_keys.ini"
    
    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(ini_content)
        print(f"\nini file generated: {output_file}")
        print(f"File size: {output_file.stat().st_size} bytes")
    except Exception as e:
        print(f"Error: Unable to write ini file: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
