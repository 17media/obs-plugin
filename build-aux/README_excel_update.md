# Locale Files Update from Excel Script

## Overview

The `update_locale_from_excel.py` script updates locale files based on translations from an Excel file. This is useful for batch updating translations provided by translators or translation services.

## Features

- **Excel Integration**: Reads translations directly from Excel files (.xlsx, .xls, .xlsm)
- **Multi-language Support**: Processes multiple languages in a single Excel file
- **Automatic Backup**: Creates timestamped backups before updating files
- **Flexible Format**: Supports Excel files with keys in first column and languages in subsequent columns
- **Auto-detection**: Automatically finds Excel files matching the expected pattern
- **Alphabetical Sorting**: Maintains alphabetical order of keys in output files
- **Detailed Reporting**: Shows statistics for each language processed

## Excel File Format

The Excel file should follow this structure:

| Key | en-US | zh-TW | ja-JP |
|-----|-------|-------|-------|
| 17Live | 17Live | 17Live | 17Live |
| Auth.Caption | 17LIVE ID Login | 17LIVE ID 登入 | 17LIVE IDログイン |
| Auth.Password | Password | 密碼 | パスワード |
| ... | ... | ... | ... |

### Requirements:
- **First Column**: Must contain the locale keys
- **First Row**: Must contain language codes (e.g., en-US, zh-TW, ja-JP)
- **Language Columns**: Each column represents translations for that language
- **File Name**: Should contain "OBS Control Panel Phase 2 Translation" for auto-detection

## Usage

### Basic Usage (Auto-detection)
```bash
python3 update_locale_from_excel.py
```
This will automatically search for Excel files in the temp directory.

### Specify Excel File
```bash
python3 update_locale_from_excel.py /path/to/translation.xlsx
```

### Custom Paths
```bash
python3 update_locale_from_excel.py [excel_file] [locale_dir] [backup_dir]
```

### Parameters
- `excel_file`: Path to the Excel file containing translations
- `locale_dir`: Directory containing locale files to update (default: `../data/locale`)
- `backup_dir`: Directory to store backups (default: `../temp`)

## Default Paths

- **Excel File**: Auto-detected in `/Users/zhuyu/workspace/mk/17live/dev/obs-17live/temp/`
- **Locale Directory**: `/Users/zhuyu/workspace/mk/17live/dev/obs-17live/data/locale`
- **Backup Directory**: `/Users/zhuyu/workspace/mk/17live/dev/obs-17live/temp`

## Dependencies

The script requires the following Python packages:
- `pandas` - For reading Excel files
- `openpyxl` - For modern Excel file support (.xlsx)

Install dependencies:
```bash
pip install pandas openpyxl
```

## Workflow

1. **Prepare Excel File**: Create or receive an Excel file with translations
2. **Place in Temp Directory**: Put the Excel file in the temp directory (or specify path)
3. **Run Script**: Execute `update_locale_from_excel.py`
4. **Review Changes**: Check the updated locale files and backups

## Example Output

```
Locale Files Update from Excel Script
Excel file: /path/to/OBS Control Panel Phase 2 Translation.xlsx
Locale directory: /path/to/data/locale
Backup directory: /path/to/temp
============================================================
Reading translations from Excel file: /path/to/translation.xlsx
Successfully read Excel file: /path/to/translation.xlsx
Excel file shape: 150 rows, 4 columns
Columns found: ['Key', 'en-US', 'zh-TW', 'ja-JP']
Key column: Key
Language columns: ['en-US', 'zh-TW', 'ja-JP']
Language en-US: 150 translations
Language zh-TW: 150 translations
Language ja-JP: 150 translations
Found translations for 3 languages

Processing en-US (150 translations)...
Current locale file has 152 keys
Backed up en-US.ini to /path/to/temp/en-US_excel_update_20250914_100822.ini
  New keys: 0
  Updated keys: 150
  Total keys after update: 152
Successfully wrote 152 keys to /path/to/data/locale/en-US.ini
  Successfully updated en-US.ini

============================================================
Update completed successfully!
Backups stored in: /path/to/temp

Please review the updated locale files for accuracy.
```

## Safety Features

- **Automatic Backups**: Original files are backed up with timestamps before any changes
- **Validation**: Checks file existence and format before processing
- **Error Handling**: Graceful error handling with informative messages
- **Preservation**: Existing keys not in Excel are preserved
- **Non-destructive**: Only updates keys that exist in the Excel file

## File Naming Convention

Backup files follow this pattern:
```
{original_name}_excel_update_{timestamp}.ini
```

Example: `en-US_excel_update_20250914_100822.ini`

## Testing

A sample Excel file can be created using the included helper script:
```bash
python3 create_sample_excel.py
```

This creates a test Excel file with sample translations for testing purposes.

## Notes

- The script preserves existing keys that are not in the Excel file
- Only keys present in the Excel file are updated
- Empty cells in Excel are ignored
- The script supports multiple Excel engines for compatibility
- All output files maintain alphabetical key ordering
- Language codes in Excel headers must match the locale file names (e.g., `zh-TW.ini`)

## Troubleshooting

### Common Issues

1. **"No Excel file found"**: Ensure the Excel file is in the temp directory and contains the expected pattern in its name
2. **"Could not read Excel file"**: Check that pandas and openpyxl are installed
3. **"No translations found"**: Verify the Excel file format matches the expected structure
4. **"Locale file does not exist"**: The script will create new locale files if they don't exist

### File Format Issues

- Ensure the first row contains language codes
- Ensure the first column contains locale keys
- Remove any merged cells or complex formatting
- Save as .xlsx format for best compatibility