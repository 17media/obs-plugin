# OBS 17Live Locale Management Tools

This directory contains a collection of tools for managing multi-language localization files for the OBS 17Live project. These tools help developers and translation teams efficiently handle locale file synchronization, updates, and translation work.

## 📁 Directory Structure

```
build-aux/
├── README.md                    # This file - General documentation
├── README_excel_update.md       # Excel update tool documentation
├── README_sync_locale.md        # Locale sync tool documentation
├── sync_locale_files.py         # Sync locale files
├── extract_locale_keys.py       # Extract locale keys
├── find_untranslated_keys.py    # Find untranslated keys
├── update_locale_from_excel.py  # Update locale files from Excel
├── create_sample_excel.py       # Create sample Excel files
└── .functions/                  # Helper function library
```

## 🛠️ Tools Overview

### 1. Locale File Synchronization Tool
- **Script**: `sync_locale_files.py`
- **Documentation**: [README_sync_locale.md](./README_sync_locale.md)
- **Function**: Synchronize locale files across different languages, ensuring all language versions contain the same keys
- **Use Case**: Automatically sync to all language files when adding new localization keys

### 2. Excel Update Tool Suite
- **Main Script**: `update_locale_from_excel.py`
- **Documentation**: [README_excel_update.md](./README_excel_update.md)
- **Function**: Batch update locale files from Excel files
- **Use Case**: Apply translations to the project after translation teams complete their work

### 3. Untranslated Content Detection Tool
- **Script**: `find_untranslated_keys.py`
- **Function**: Automatically detect content that needs translation and generate Excel files
- **Features**: 
  - Identify untranslated items where key equals value
  - Exclude special cases (like `17Live="17Live"`)
  - Generate Excel format convenient for translation

### 4. Helper Tools
- **`extract_locale_keys.py`**: Extract all keys from locale files
- **`create_sample_excel.py`**: Create sample Excel files for testing

## 🚀 Quick Start

### Basic Workflow

1. **Detect Untranslated Content**
   ```bash
   python3 find_untranslated_keys.py
   ```
   Generate Excel file containing untranslated keys

2. **Translation Work**
   - Open the generated Excel file
   - Fill in translation content in corresponding language columns
   - Save the file

3. **Apply Translations**
   ```bash
   python3 update_locale_from_excel.py path/to/translated.xlsx
   ```
   Batch update locale files with translation content

4. **Sync Check**
   ```bash
   python3 sync_locale_files.py
   ```
   Ensure all language files have consistent structure

### Advanced Usage

- **Process Specific Directory**
  ```bash
  python3 find_untranslated_keys.py /custom/locale/path
  ```

- **Custom Output File**
  ```bash
  python3 find_untranslated_keys.py /locale/path /output/file.xlsx
  ```

## 📋 Supported Languages

Currently supported language codes:
- `en-US` - English (United States)
- `ja-JP` - Japanese (Japan)
- `zh-CN` - Simplified Chinese (China)
- `zh-TW` - Traditional Chinese (Taiwan)

## 📁 File Formats

### Locale File Format (.ini)
```ini
# Comments
Key1="Value1"
Key2="Value2"
17Live="17Live"
```

### Excel File Format
| Key | en-US | ja-JP | zh-CN | zh-TW |
|-----|-------|-------|-------|-------|
| CustomEvent.Error.Title | | | | |
| Auth.LoginFailed | | | | |

## 🔧 Dependencies

- Python 3.6+
- pandas
- openpyxl

Install dependencies:
```bash
pip install pandas openpyxl
```

## 📝 Best Practices

1. **Backup Mechanism**: All tools automatically create backup files stored in the `temp/` directory
2. **Format Preservation**: Tools maintain original quote formatting and file structure
3. **Incremental Updates**: Support incremental updates without affecting existing translations
4. **Error Handling**: Provide detailed error messages and handling suggestions

## 🚨 Important Notes

- Ensure locale files are not being used by other programs before running
- Recommend testing before committing changes to version control
- Excel files should use UTF-8 encoding to support multi-language characters
- Special keys (like `17Live`) are automatically excluded from translation detection

## 🔗 Related Documentation

- [Excel Update Tool Detailed Documentation](./README_excel_update.md)
- [Locale Sync Tool Detailed Documentation](./README_sync_locale.md)
- [Main Project Documentation](../README.md)

## 🤝 Contributing Guidelines

To add new language support or feature improvements:
1. Ensure new features are compatible with existing tools
2. Update relevant documentation
3. Add appropriate test cases
4. Follow existing code style

---

**Maintainers**: OBS 17Live Development Team  
**Last Updated**: 2025-09-14