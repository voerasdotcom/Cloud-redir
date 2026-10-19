# CloudRedir: Nintendo Switch Save Data Sync

## Overview
CloudRedir is a homebrew solution for the Nintendo Switch that automatically backs up game save data to a cloud provider (e.g., Google Drive) in real-time. It consists of two main components:
1. A background sysmodule that runs in the Horizon OS environment, monitors game state, and handles save data synchronization.
2. A frontend configuration application that allows users to set up OAuth2 authentication and configure sync settings.

## Architecture

### Background Sysmodule
- Runs as a sysmodule (`.nso`) with low memory footprint (target: 1-4MB).
- Uses Horizon OS services (`pm:` and `ns:`) to monitor title lifecycle.
- When a game exits, it copies the save data to a temporary location and uploads it to the cloud.
- Implements a lightweight HTTP client for cloud API communication (using libcurl or raw sockets with mbedTLS).
- Securely stores OAuth tokens (in practice, should be encrypted; current skeleton uses plaintext for simplicity).

### Frontend Application
- Runs as a regular homebrew application (`.nro`).
- Implements OAuth2 Device Authorization Flow for Google Drive (no web browser required on Switch).
- Writes configuration (including access token) to a file in the sysmodule's save data location.
- Provides a simple console-based UI for configuration.

## Directory Structure
```
CloudRedir/
├── sysmodule/
│   ├── src/
│   │   └── main.cpp          # Sysmodule main entry point and logic
│   ├── include/
│   │   └── ...               # Header files
│   └── CMakeLists.txt        # Build configuration for sysmodule
├── frontend/
│   ├── src/
│   │   └── main.cpp          # Frontend main entry point and logic
│   ├── include/
│   │   └── ...               # Header files
│   └── CMakeLists.txt        # Build configuration for frontend
└── README.md                 # This file
```

## Building
Both components require the devkitPro toolchain and the Nintendo Switch SDK (libnx). Additionally, libcurl and mbedTLS (or mbedtls) are needed for HTTPS communication.

### Prerequisites
- devkitPro (with Switch support)
- CMake 3.16 or higher
- libnx (installed via devkitPro)
- libcurl (built for Switch-a64)
- mbedTLS (built for Switch-a64)

### Build Steps
1. Set up the devkitPro environment (ensure `switch` rules are in your PATH).
2. For each component (sysmodule and frontend):
   ```bash
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=<path-to-devkitPro>/switch.cmake
   make
   ```
3. The resulting `.nso` (sysmodule) and `.nro` (frontend) will be in the build directories.

## Installation
- Copy the sysmodule `.nso` to the appropriate location on your Switch SD card (e.g., `/atmosphere/contents/0100000000001000/` for sysmodules, or use a sysmodule loader).
- Copy the frontend `.nro` to the Switch SD card and launch it via the Homebrew Launcher to configure the sysmodule.

## Configuration
1. Launch the frontend application.
2. Follow the on-screen instructions to authorize Google Drive via the device authorization flow.
3. The frontend will save a configuration file (`sd:/sysmodules/cloudredir/config.json`) that the sysmodule reads on startup.

## Notes
- This is a prototype skeleton. Production implementation requires:
  - Secure token storage (encryption).
  - Robust error handling and retry logic.
  - Efficient file copying (to avoid excessive memory usage).
  - Minimal HTTP client implementation to stay within sysmodule memory limits.
  - Proper synchronization mechanisms to avoid race conditions.
- The sysmodule must be designed to never exceed its memory budget (typically 1-4MB). Avoid dynamic allocation where possible, and use static buffers.

## License
This project is licensed under the MIT License - see the LICENSE file for details.

## Disclaimer
This project is for educational and homebrew purposes only. It is not affiliated with Nintendo or Google. Use at your own risk.