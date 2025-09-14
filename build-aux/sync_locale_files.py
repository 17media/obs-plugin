#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Locale Files Synchronization Script

This script synchronizes locale files with a standard reference file.
It adds missing keys, removes extra keys, backs up original files,
and sorts the output alphabetically.

Usage:
    python sync_locale_files.py [reference_file] [locale_dir] [backup_dir]
"""

import os
import sys
import shutil
from pathlib import Path
from typing import Dict, Set, List
from datetime import datetime
import configparser


def read_ini_file(file_path: Path) -> Dict[str, str]:
    """
    Read an INI file and return a dictionary of key-value pairs.
    
    Args:
        file_path: Path to the INI file
        
    Returns:
        Dictionary containing key-value pairs from the INI file
    """
    result = {}
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                
                # Skip empty lines and comments
                if not line or line.startswith('#'):
                    continue
                    
                # Look for key=value pairs
                if '=' in line:
                    key, value = line.split('=', 1)
                    key = key.strip()
                    value = value.strip()
                    
                    if key:  # Only add non-empty keys
                        result[key] = value
                        
        return result
        
    except Exception as e:
        print(f"Error reading file {file_path}: {e}")
        return {}


def write_ini_file(file_path: Path, data: Dict[str, str], header_comment: str = None):
    """
    Write data to an INI file with sorted keys.
    
    Args:
        file_path: Path to write the INI file
        data: Dictionary of key-value pairs to write
        header_comment: Optional header comment to add at the top
    """
    try:
        with open(file_path, 'w', encoding='utf-8') as f:
            if header_comment:
                f.write(header_comment + '\n\n')
                
            # Sort keys alphabetically
            sorted_keys = sorted(data.keys())
            for key in sorted_keys:
                f.write(f"{key}={data[key]}\n")
                
        print(f"Successfully wrote {len(data)} keys to {file_path}")
        
    except Exception as e:
        print(f"Error writing file {file_path}: {e}")


def backup_file(source_path: Path, backup_dir: Path) -> Path:
    """
    Create a backup of the source file in the backup directory.
    
    Args:
        source_path: Path to the file to backup
        backup_dir: Directory to store the backup
        
    Returns:
        Path to the backup file
    """
    try:
        # Create backup directory if it doesn't exist
        backup_dir.mkdir(parents=True, exist_ok=True)
        
        # Generate backup filename with timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_filename = f"{source_path.stem}_{timestamp}{source_path.suffix}"
        backup_path = backup_dir / backup_filename
        
        # Copy the file
        shutil.copy2(source_path, backup_path)
        print(f"Backed up {source_path.name} to {backup_path}")
        
        return backup_path
        
    except Exception as e:
        print(f"Error backing up file {source_path}: {e}")
        return None


def sync_locale_file(reference_data: Dict[str, str], locale_file: Path, backup_dir: Path):
    """
    Synchronize a locale file with the reference data.
    
    Args:
        reference_data: Dictionary containing the reference key-value pairs
        locale_file: Path to the locale file to synchronize
        backup_dir: Directory to store backups
    """
    print(f"\nProcessing {locale_file.name}...")
    
    # Read current locale file
    current_data = read_ini_file(locale_file)
    
    if not current_data:
        print(f"Warning: Could not read {locale_file.name}, skipping...")
        return
        
    # Create backup
    backup_file(locale_file, backup_dir)
    
    # Get reference keys
    reference_keys = set(reference_data.keys())
    current_keys = set(current_data.keys())
    
    # Find missing and extra keys
    missing_keys = reference_keys - current_keys
    extra_keys = current_keys - reference_keys
    
    print(f"  Current keys: {len(current_keys)}")
    print(f"  Reference keys: {len(reference_keys)}")
    print(f"  Missing keys: {len(missing_keys)}")
    print(f"  Extra keys: {len(extra_keys)}")
    
    if missing_keys:
        print(f"  Adding missing keys: {', '.join(sorted(missing_keys))}")
        
    if extra_keys:
        print(f"  Removing extra keys: {', '.join(sorted(extra_keys))}")
    
    # Create synchronized data
    synchronized_data = {}
    
    # Add all reference keys
    for key in reference_keys:
        if key in current_data:
            # Keep existing translation
            synchronized_data[key] = current_data[key]
        else:
            # Add missing key with reference value (placeholder)
            synchronized_data[key] = reference_data[key]
    
    # Generate header comment
    header_comment = f"# OBS 17Live Plugin Locale Keys\n# Synchronized on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n# Please provide appropriate translations for each key"
    
    # Write synchronized file
    write_ini_file(locale_file, synchronized_data, header_comment)
    
    print(f"  Synchronization completed for {locale_file.name}")


def main():
    """
    Main function to synchronize locale files.
    """
    # Default paths
    default_reference = "/Users/zhuyu/workspace/mk/17live/dev/obs-17live/temp/extracted_locale_keys.ini"
    default_locale_dir = "/Users/zhuyu/workspace/mk/17live/dev/obs-17live/data/locale"
    default_backup_dir = "/Users/zhuyu/workspace/mk/17live/dev/obs-17live/temp"
    
    # Parse command line arguments
    if len(sys.argv) > 1:
        reference_file = Path(sys.argv[1])
    else:
        reference_file = Path(default_reference)
        
    if len(sys.argv) > 2:
        locale_dir = Path(sys.argv[2])
    else:
        locale_dir = Path(default_locale_dir)
        
    if len(sys.argv) > 3:
        backup_dir = Path(sys.argv[3])
    else:
        backup_dir = Path(default_backup_dir)
    
    print(f"Locale Files Synchronization Script")
    print(f"Reference file: {reference_file}")
    print(f"Locale directory: {locale_dir}")
    print(f"Backup directory: {backup_dir}")
    print("=" * 50)
    
    # Check if reference file exists
    if not reference_file.exists():
        print(f"Error: Reference file {reference_file} does not exist!")
        sys.exit(1)
        
    # Check if locale directory exists
    if not locale_dir.exists():
        print(f"Error: Locale directory {locale_dir} does not exist!")
        sys.exit(1)
    
    # Read reference file
    print(f"Reading reference file: {reference_file}")
    reference_data = read_ini_file(reference_file)
    
    if not reference_data:
        print("Error: Could not read reference file or it's empty!")
        sys.exit(1)
        
    print(f"Reference file contains {len(reference_data)} keys")
    
    # Find all .ini files in locale directory
    locale_files = list(locale_dir.glob("*.ini"))
    
    if not locale_files:
        print(f"Warning: No .ini files found in {locale_dir}")
        return
        
    print(f"Found {len(locale_files)} locale files to process")
    
    # Process each locale file
    for locale_file in sorted(locale_files):
        sync_locale_file(reference_data, locale_file, backup_dir)
    
    print("\n" + "=" * 50)
    print("Synchronization completed successfully!")
    print(f"Backups stored in: {backup_dir}")


if __name__ == "__main__":
    main()