# Locale Files Synchronization Script

## Overview

The `sync_locale_files.py` script synchronizes locale files with a standard reference file. It ensures all locale files have the same keys, adds missing keys, removes extra keys, backs up original files, and sorts the output alphabetically.

## Features

- **Key Synchronization**: Adds missing keys and removes extra keys based on a reference file
- **Automatic Backup**: Creates timestamped backups of original files before modification
- **Alphabetical Sorting**: Sorts all keys alphabetically in the output files
- **Multi-language Support**: Processes all `.ini` files in the locale directory
- **Detailed Reporting**: Shows statistics for each file processed

## Usage

### Basic Usage (with default paths)
```bash
python3 sync_locale_files.py
```

### Custom Paths
```bash
python3 sync_locale_files.py [reference_file] [locale_dir] [backup_dir]
```

### Parameters
- `reference_file`: Path to the standard INI file (default: `../temp/extracted_locale_keys.ini`)
- `locale_dir`: Directory containing locale files to sync (default: `../data/locale`)
- `backup_dir`: Directory to store backups (default: `../temp`)

## Default Paths

- **Reference File**: `/Users/zhuyu/workspace/mk/17live/dev/obs-17live/temp/extracted_locale_keys.ini`
- **Locale Directory**: `/Users/zhuyu/workspace/mk/17live/dev/obs-17live/data/locale`
- **Backup Directory**: `/Users/zhuyu/workspace/mk/17live/dev/obs-17live/temp`

## Workflow

1. **Generate Reference File**: First run `extract_locale_keys.py` to generate the reference file
2. **Run Synchronization**: Execute `sync_locale_files.py` to sync all locale files
3. **Review Changes**: Check the output and backup files if needed

## Example Output

```
Locale Files Synchronization Script
Reference file: /path/to/extracted_locale_keys.ini
Locale directory: /path/to/data/locale
Backup directory: /path/to/temp
==================================================
Reading reference file: /path/to/extracted_locale_keys.ini
Reference file contains 152 keys
Found 4 locale files to process

Processing en-US.ini...
Backed up en-US.ini to /path/to/temp/en-US_20250914_095449.ini
  Current keys: 150
  Reference keys: 152
  Missing keys: 2
  Extra keys: 0
  Adding missing keys: New.Key1, New.Key2
Successfully wrote 152 keys to /path/to/data/locale/en-US.ini
  Synchronization completed for en-US.ini

==================================================
Synchronization completed successfully!
Backups stored in: /path/to/temp
```

## File Format

The script handles INI files with the following format:
- Comments start with `#`
- Key-value pairs in format: `key=value`
- Empty lines are ignored
- No sections required

## Safety Features

- **Automatic Backups**: Original files are backed up with timestamps before modification
- **Validation**: Checks file existence before processing
- **Error Handling**: Graceful error handling with informative messages
- **Preservation**: Existing translations are preserved when keys match

## Notes

- The script preserves existing translations for matching keys
- New keys are added with the same value as in the reference file (as placeholders)
- All output files are sorted alphabetically by key
- Backup files include timestamps to prevent conflicts