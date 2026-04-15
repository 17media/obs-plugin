# OBS 17LIVE Plugin

A plugin for OBS Studio that enables seamless integration with 17LIVE streaming platform. This plugin allows streamers to manage their 17LIVE streams directly from OBS Studio and includes features for stream configuration and chat integration.

## Features

- Direct integration with 17LIVE streaming platform
- Stream settings management (title, hashtags)
- Automatic stream key generation and management
- Built-in chat browser source for 17LIVE chat integration
- Secure credential management

## Installation

The Steam version of OBS is also OBS Studio; this plugin is compatible with both the official and Steam editions by installing into OBS's plugin directory.
1. Download the latest release of the plugin
2. Install:
   - macOS: run the `.pkg` (installs to `/Library/Application Support/obs-studio/plugins`, requires admin password)
   - Windows: run the `.exe` (installs to `%ProgramData%\obs-studio\plugins`)
   - non-installer zip:
     - macOS: unzip and copy `obs-17live.plugin` to `/Library/Application Support/obs-studio/plugins`
     - Windows: unzip and copy `obs-17live` to `%ProgramData%\obs-studio\plugins`
3. Restart OBS Studio
4. The plugin will appear under Tools menu as "17LIVE Settings"

## Usage

### Stream Configuration

1. Open OBS Studio
2. Go to Tools -> 17LIVE Settings
3. Log in with your 17LIVE account credentials
4. Configure your stream settings:
   - Set stream title
   - Add hashtags
   - Generate a stream key
5. Click "Save Settings" to apply changes

### Chat Integration

1. In the 17LIVE Settings dialog, click "Create Chat Browser Source"
2. A new browser source will be automatically added to your current scene
3. The chat source will display live chat from your 17LIVE stream
4. You can resize and position the chat overlay as needed

### Stream Key Management

1. Log in to your account in the 17LIVE Settings
2. Click "Generate Stream Key" to get a new stream key
3. The stream key will be automatically saved
4. Use this stream key in your OBS stream settings

## Security

- Your login credentials are securely stored
- Stream keys are managed securely
- All communication with 17LIVE servers is encrypted

## Troubleshooting

- If the chat browser source is not working, ensure you are logged in
- For stream key issues, try generating a new key
- Make sure you have stable internet connection

## Support

For issues and feature requests, please create an issue in the repository.

## License

This project is licensed under the terms specified in the LICENSE file.
