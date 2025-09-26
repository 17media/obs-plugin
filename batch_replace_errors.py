#!/usr/bin/env python3
"""
批量替换错误处理代码的脚本
"""

import re
import sys

def replace_error_patterns(file_path):
    """替换文件中的错误处理模式"""
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 定义替换模式
    patterns = [
        # 模式1: lastErrorMessage = QString::fromStdString(json_out["errorCode"].get<std::string>()) + " " + QString::fromStdString(json_out["errorMessage"].get<std::string>());
        {
            'pattern': r'lastErrorMessage\s*=\s*QString::fromStdString\(json_out\["errorCode"\]\.get<std::string>\(\)\)\s*\+\s*"\s*"\s*\+\s*QString::fromStdString\(json_out\["errorMessage"\]\.get<std::string>\(\)\);',
            'replacement': 'lastErrorMessage = buildErrorMessage(json_out, "API call failed");'
        },
        # 模式2: lastErrorMessage = QString::fromStdString(json_out_resp["errorCode"].get<std::string>()) + " " + QString::fromStdString(json_out_resp["errorMessage"].get<std::string>());
        {
            'pattern': r'lastErrorMessage\s*=\s*QString::fromStdString\(json_out_resp\["errorCode"\]\.get<std::string>\(\)\)\s*\+\s*"\s*"\s*\+\s*QString::fromStdString\(json_out_resp\["errorMessage"\]\.get<std::string>\(\)\);',
            'replacement': 'lastErrorMessage = buildErrorMessage(json_out_resp, "API call failed");'
        }
    ]
    
    # 应用替换
    modified = False
    for pattern_info in patterns:
        if re.search(pattern_info['pattern'], content):
            content = re.sub(pattern_info['pattern'], pattern_info['replacement'], content)
            modified = True
            print(f"Applied pattern: {pattern_info['pattern'][:50]}...")
    
    if modified:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"File {file_path} has been updated.")
        return True
    else:
        print(f"No patterns found in {file_path}")
        return False

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 batch_replace_errors.py <file_path>")
        sys.exit(1)
    
    file_path = sys.argv[1]
    replace_error_patterns(file_path)