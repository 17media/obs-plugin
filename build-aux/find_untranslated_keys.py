#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Find Untranslated Keys Script

This script searches for locale keys where the key and value are identical,
which typically indicates untranslated content. It generates an Excel file
with these keys for translation purposes.

Usage:
    python find_untranslated_keys.py [locale_dir] [output_file]
"""

import os
import sys
import pandas as pd
from pathlib import Path
from typing import Dict, List, Set
from datetime import datetime


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


def remove_quotes(value: str) -> str:
    """
    Remove surrounding quotes from a value if present.
    
    Args:
        value: The value string
        
    Returns:
        Value without surrounding quotes
    """
    if value.startswith('"') and value.endswith('"') and len(value) >= 2:
        return value[1:-1]
    return value


def find_untranslated_keys(locale_dir: Path) -> Dict[str, Dict[str, str]]:
    """
    Find keys where the key and value are identical (indicating untranslated content).
    
    Args:
        locale_dir: Directory containing locale files
        
    Returns:
        Dictionary with language codes as keys and untranslated key-value pairs as values
    """
    untranslated = {}
    
    # Get all .ini files in the locale directory
    ini_files = list(locale_dir.glob('*.ini'))
    
    if not ini_files:
        print(f"No .ini files found in {locale_dir}")
        return untranslated
    
    print(f"Found {len(ini_files)} locale files:")
    for file_path in ini_files:
        print(f"  - {file_path.name}")
    
    # Process each locale file
    for file_path in ini_files:
        language_code = file_path.stem  # e.g., 'en-US' from 'en-US.ini'
        
        print(f"\nProcessing {language_code}...")
        
        # Read the locale file
        locale_data = read_ini_file(file_path)
        
        if not locale_data:
            print(f"  No data found in {file_path.name}")
            continue
        
        print(f"  Total keys: {len(locale_data)}")
        
        # Find untranslated keys (where key == value after removing quotes)
        untranslated_keys = {}
        
        for key, value in locale_data.items():
            # Remove quotes from value for comparison
            clean_value = remove_quotes(value)
            
            # Check if key equals value (indicating untranslated)
            # Exclude the special case of 17Live="17Live"
            if key == clean_value and key != "17Live":
                untranslated_keys[key] = value
        
        if untranslated_keys:
            untranslated[language_code] = untranslated_keys
            print(f"  Untranslated keys found: {len(untranslated_keys)}")
            
            # Show first few examples
            examples = list(untranslated_keys.items())[:3]
            for k, v in examples:
                print(f"    {k}={v}")
            if len(untranslated_keys) > 3:
                print(f"    ... and {len(untranslated_keys) - 3} more")
        else:
            print(f"  No untranslated keys found")
    
    return untranslated


def create_excel_file(untranslated_data: Dict[str, Dict[str, str]], output_file: Path, source_dir: Path):
    """
    Create an Excel file with untranslated keys.
    
    Args:
        untranslated_data: Dictionary with language codes and their untranslated keys
        output_file: Path to the output Excel file
    """
    if not untranslated_data:
        print("No untranslated keys found. No Excel file will be created.")
        return
    
    # Collect all unique keys across all languages
    all_keys = set()
    for lang_data in untranslated_data.values():
        all_keys.update(lang_data.keys())
    
    all_keys = sorted(list(all_keys))
    
    print(f"\nCreating Excel file with {len(all_keys)} unique untranslated keys...")
    
    # Create DataFrame
    data = {'Key': all_keys}
    
    # Add columns for each language (only translation columns)
    languages = sorted(untranslated_data.keys())
    for lang in languages:
        # Add empty column for translation
        data[lang] = [''] * len(all_keys)
    
    df = pd.DataFrame(data)
    
    # Create output directory if it doesn't exist
    output_file.parent.mkdir(parents=True, exist_ok=True)
    
    # Write to Excel
    try:
        with pd.ExcelWriter(output_file, engine='openpyxl') as writer:
            # Write main sheet
            df.to_excel(writer, sheet_name='Untranslated Keys', index=False)
            
            # Write summary sheet
            summary_data = {
                'Language': languages,
                'Untranslated Count': [len(untranslated_data[lang]) for lang in languages]
            }
            summary_df = pd.DataFrame(summary_data)
            summary_df.to_excel(writer, sheet_name='Summary', index=False)
            
            # Add metadata sheet
            metadata = {
                'Property': ['Generated Date', 'Total Unique Keys', 'Languages Processed', 'Source Directory'],
                'Value': [
                    datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                    len(all_keys),
                    ', '.join(languages),
                    str(source_dir)
                ]
            }
            metadata_df = pd.DataFrame(metadata)
            metadata_df.to_excel(writer, sheet_name='Metadata', index=False)
        
        print(f"Excel file created successfully: {output_file}")
        print(f"File contains:")
        print(f"  - {len(all_keys)} unique untranslated keys")
        print(f"  - {len(languages)} languages: {', '.join(languages)}")
        print(f"  - 3 sheets: 'Untranslated Keys', 'Summary', 'Metadata'")
        
    except Exception as e:
        print(f"Error creating Excel file: {e}")


def main():
    """
    Main function to find untranslated keys and create Excel file.
    """
    # Default paths
    default_locale_dir = "/Users/zhuyu/workspace/mk/17live/dev/obs-17live/data/locale"
    default_output_dir = "/Users/zhuyu/workspace/mk/17live/dev/obs-17live/temp"
    
    # Parse command line arguments
    if len(sys.argv) > 1:
        locale_dir = Path(sys.argv[1])
    else:
        locale_dir = Path(default_locale_dir)
        
    if len(sys.argv) > 2:
        output_file = Path(sys.argv[2])
    else:
        # Generate default output filename with timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_filename = f"Untranslated_Keys_{timestamp}.xlsx"
        output_file = Path(default_output_dir) / output_filename
    
    print(f"Find Untranslated Keys Script")
    print(f"Locale directory: {locale_dir}")
    print(f"Output file: {output_file}")
    print("=" * 60)
    
    # Check if locale directory exists
    if not locale_dir.exists():
        print(f"Error: Locale directory {locale_dir} does not exist!")
        sys.exit(1)
        
    if not locale_dir.is_dir():
        print(f"Error: {locale_dir} is not a directory!")
        sys.exit(1)
    
    # Find untranslated keys
    print(f"Searching for untranslated keys in {locale_dir}...")
    untranslated_data = find_untranslated_keys(locale_dir)
    
    if not untranslated_data:
        print("\nNo untranslated keys found in any locale files.")
        print("All keys appear to be properly translated!")
        return
    
    # Create Excel file
    create_excel_file(untranslated_data, output_file, locale_dir)
    
    print("\n" + "=" * 60)
    print("Process completed successfully!")
    print(f"\nNext steps:")
    print(f"1. Open the Excel file: {output_file}")
    print(f"2. Fill in the language columns (en-US, ja-JP, zh-CN, zh-TW) with proper translations")
    print(f"3. Use the update_locale_from_excel.py script to apply the translations")


if __name__ == "__main__":
    main()