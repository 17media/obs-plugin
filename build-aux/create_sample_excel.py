#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Create Sample Excel File for Testing

This script creates a sample Excel file with translation data
to test the update_locale_from_excel.py script.
"""

import pandas as pd
from pathlib import Path

def create_sample_excel():
    """
    Create a sample Excel file with translation data.
    """
    # Sample translation data
    data = {
        'Key': [
            '17Live',
            'Auth.Caption',
            'Auth.Error01',
            'Auth.Password',
            'Auth.SignIn',
            'Menu.Broadcast',
            'Menu.ChatRoom',
            'Live.Settings.Title',
            'Update.NewVersionFound',
            'CustomEvent.Title'
        ],
        'en-US': [
            '17Live',
            '17LIVE ID Login',
            'Username or password is incorrect',
            'Password',
            'Sign In',
            'Broadcast',
            'Chat Room',
            'Live Settings',
            'New Version Available',
            'Custom Event Title'
        ],
        'zh-TW': [
            '17Live',
            '17LIVE ID 登入',
            '用戶名或密碼不正確',
            '密碼',
            '登入',
            '廣播',
            '聊天室',
            '直播設定',
            '發現新版本',
            '自定義活動標題'
        ],
        'ja-JP': [
            '17Live',
            '17LIVE IDログイン',
            'ユーザー名またはパスワードが正しくありません',
            'パスワード',
            'サインイン',
            'ブロードキャスト',
            'チャットルーム',
            'ライブ設定',
            '新しいバージョンが利用可能',
            'カスタムイベントタイトル'
        ]
    }
    
    # Create DataFrame
    df = pd.DataFrame(data)
    
    # Create temp directory if it doesn't exist
    temp_dir = Path('/Users/zhuyu/workspace/mk/17live/dev/obs-17live/temp')
    temp_dir.mkdir(parents=True, exist_ok=True)
    
    # Save to Excel file
    excel_path = temp_dir / 'OBS Control Panel Phase 2 Translation.xlsx'
    df.to_excel(excel_path, index=False, engine='openpyxl')
    
    print(f"Sample Excel file created: {excel_path}")
    print(f"File contains {len(df)} rows and {len(df.columns)} columns")
    print(f"Languages: {', '.join(df.columns[1:])}")
    
    return excel_path

if __name__ == "__main__":
    create_sample_excel()