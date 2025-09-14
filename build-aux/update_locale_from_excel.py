#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Locale Files Update from Excel Script

This script updates locale files based on translations from an Excel file.
The Excel file should have keys in the first column and translations in subsequent columns.
The first row determines the language codes (e.g., zh-TW, ja-JP, en-US).

Usage:
    python update_locale_from_excel.py [excel_file] [locale_dir] [backup_dir]
"""

import os
import sys
import shutil
import pandas as pd
from pathlib import Path
from typing import Dict, List, Optional
from datetime import datetime


def backup_file(source_path: Path, backup_dir: Path) -> Optional[Path]:
    """
    Create a backup of the source file in the backup directory.
    
    Args:
        source_path: Path to the file to backup
        backup_dir: Directory to store the backup
        
    Returns:
        Path to the backup file, or None if backup failed
    """
    try:
        # Create backup directory if it doesn't exist
        backup_dir.mkdir(parents=True, exist_ok=True)
        
        # Generate backup filename with timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_filename = f"{source_path.stem}_excel_update_{timestamp}{source_path.suffix}"
        backup_path = backup_dir / backup_filename
        
        # Copy the file
        shutil.copy2(source_path, backup_path)
        print(f"Backed up {source_path.name} to {backup_path}")
        
        return backup_path
        
    except Exception as e:
        print(f"Error backing up file {source_path}: {e}")
        return None


def read_excel_translations(excel_path: Path) -> Dict[str, Dict[str, str]]:
    """
    Read translations from Excel file.
    
    Args:
        excel_path: Path to the Excel file
        
    Returns:
        Dictionary with language codes as keys and key-value translation pairs as values
        Format: {"zh-TW": {"key1": "value1", "key2": "value2"}, ...}
    """
    try:
        # Try to read Excel file with different engines
        try:
            df = pd.read_excel(excel_path, engine='openpyxl')
        except:
            try:
                df = pd.read_excel(excel_path, engine='xlrd')
            except:
                df = pd.read_excel(excel_path)
        
        print(f"Successfully read Excel file: {excel_path}")
        print(f"Excel file shape: {df.shape[0]} rows, {df.shape[1]} columns")
        
        # Get column names (first row should contain language codes)
        columns = df.columns.tolist()
        print(f"Columns found: {columns}")
        
        # First column should be keys
        key_column = columns[0]
        language_columns = columns[1:]
        
        # Remove any unnamed columns or columns with NaN names
        language_columns = [col for col in language_columns if not str(col).startswith('Unnamed') and pd.notna(col)]
        
        print(f"Key column: {key_column}")
        print(f"Language columns: {language_columns}")
        
        # Build translations dictionary
        translations = {}
        
        for lang_col in language_columns:
            translations[lang_col] = {}
            
            for index, row in df.iterrows():
                key = row[key_column]
                value = row[lang_col]
                
                # Skip rows with empty keys or values
                if pd.notna(key) and pd.notna(value) and str(key).strip() and str(value).strip():
                    key = str(key).strip()
                    value = str(value).strip()
                    translations[lang_col][key] = value
            
            print(f"Language {lang_col}: {len(translations[lang_col])} translations")
        
        return translations
        
    except Exception as e:
        print(f"Error reading Excel file {excel_path}: {e}")
        return {}


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
                    # Don't strip quotes from value, preserve original format
                    value = value.rstrip()  # Only remove trailing whitespace
                    
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
                value = data[key]
                # Always add quotes to values to maintain consistency with original format
                if not (value.startswith('"') and value.endswith('"')):
                    value = f'"{value}"'
                f.write(f"{key}={value}\n")
                
        print(f"Successfully wrote {len(data)} keys to {file_path}")
        
    except Exception as e:
        print(f"Error writing file {file_path}: {e}")


def update_locale_file(language_code: str, translations: Dict[str, str], locale_dir: Path, backup_dir: Path):
    """
    Update a locale file with translations from Excel.
    
    Args:
        language_code: Language code (e.g., 'zh-TW', 'ja-JP')
        translations: Dictionary of key-value translation pairs
        locale_dir: Directory containing locale files
        backup_dir: Directory to store backups
    """
    # Determine the locale file name
    locale_file = locale_dir / f"{language_code}.ini"
    
    print(f"\nProcessing {language_code} ({len(translations)} translations)...")
    
    # Check if locale file exists
    if not locale_file.exists():
        print(f"Warning: Locale file {locale_file} does not exist. Creating new file.")
        current_data = {}
    else:
        # Read current locale file
        current_data = read_ini_file(locale_file)
        print(f"Current locale file has {len(current_data)} keys")
        
        # Create backup
        backup_file(locale_file, backup_dir)
    
    # Update translations
    updated_data = current_data.copy()
    new_keys = 0
    updated_keys = 0
    
    for key, value in translations.items():
        if key not in updated_data:
            new_keys += 1
        elif updated_data[key] != value:
            updated_keys += 1
        updated_data[key] = value
    
    print(f"  New keys: {new_keys}")
    print(f"  Updated keys: {updated_keys}")
    print(f"  Total keys after update: {len(updated_data)}")
    
    # Generate header comment
    header_comment = f"# OBS 17Live Plugin Locale Keys - {language_code}\n# Updated from Excel on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n# Please review the translations for accuracy"
    
    # Write updated file
    write_ini_file(locale_file, updated_data, header_comment)
    
    print(f"  Successfully updated {locale_file.name}")


def find_excel_file(temp_dir: Path, pattern: str = "OBS Control Panel Phase 2 Translation") -> Optional[Path]:
    """
    Find Excel file in temp directory matching the pattern.
    
    Args:
        temp_dir: Directory to search in
        pattern: Pattern to match in filename
        
    Returns:
        Path to the Excel file, or None if not found
    """
    if not temp_dir.exists():
        return None
        
    # Look for Excel files with the pattern
    excel_extensions = ['*.xlsx', '*.xls', '*.xlsm']
    
    for ext in excel_extensions:
        for file_path in temp_dir.glob(ext):
            if pattern.lower() in file_path.name.lower():
                return file_path
    
    return None


def main():
    """
    Main function to update locale files from Excel.
    """
    # Default paths
    default_temp_dir = "/Users/zhuyu/workspace/mk/17live/dev/obs-17live/temp"
    default_locale_dir = "/Users/zhuyu/workspace/mk/17live/dev/obs-17live/data/locale"
    default_backup_dir = "/Users/zhuyu/workspace/mk/17live/dev/obs-17live/temp"
    
    # Parse command line arguments
    if len(sys.argv) > 1:
        excel_file = Path(sys.argv[1])
    else:
        # Try to find Excel file automatically
        temp_dir = Path(default_temp_dir)
        excel_file = find_excel_file(temp_dir)
        if not excel_file:
            print(f"Error: No Excel file found in {temp_dir}")
            print("Please specify the Excel file path as the first argument.")
            sys.exit(1)
        
    if len(sys.argv) > 2:
        locale_dir = Path(sys.argv[2])
    else:
        locale_dir = Path(default_locale_dir)
        
    if len(sys.argv) > 3:
        backup_dir = Path(sys.argv[3])
    else:
        backup_dir = Path(default_backup_dir)
    
    print(f"Locale Files Update from Excel Script")
    print(f"Excel file: {excel_file}")
    print(f"Locale directory: {locale_dir}")
    print(f"Backup directory: {backup_dir}")
    print("=" * 60)
    
    # Check if Excel file exists
    if not excel_file.exists():
        print(f"Error: Excel file {excel_file} does not exist!")
        sys.exit(1)
        
    # Check if locale directory exists
    if not locale_dir.exists():
        print(f"Error: Locale directory {locale_dir} does not exist!")
        sys.exit(1)
    
    # Read translations from Excel
    print(f"Reading translations from Excel file: {excel_file}")
    translations = read_excel_translations(excel_file)
    
    if not translations:
        print("Error: Could not read translations from Excel file or it's empty!")
        sys.exit(1)
    
    print(f"Found translations for {len(translations)} languages")
    
    # Update each language file
    for language_code, lang_translations in translations.items():
        if lang_translations:  # Only process if there are translations
            update_locale_file(language_code, lang_translations, locale_dir, backup_dir)
        else:
            print(f"\nSkipping {language_code}: No translations found")
    
    print("\n" + "=" * 60)
    print("Update completed successfully!")
    print(f"Backups stored in: {backup_dir}")
    print("\nPlease review the updated locale files for accuracy.")


if __name__ == "__main__":
    main()