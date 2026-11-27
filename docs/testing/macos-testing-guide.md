# QtDMM macOS Testing Guide

Thank you for helping test QtDMM on macOS! This guide provides structured test scenarios to help you systematically evaluate the application on real Mac hardware. Your feedback is invaluable since none of the developers have access to Mac hardware.

## Getting Started

### Prerequisites
- A Mac (Apple Silicon or Intel) running macOS 13 or newer
- QtDDM installation from either:
  - Latest release: [https://github.com/qtdmm/QtDMM/releases/latest](https://github.com/qtdmm/QtDMM/releases/latest)
  - Development build: [GitHub Actions workflow artifacts](https://github.com/qtdmm/QtDMM/actions/workflows/cmake-mac-platform.yml) (requires GitHub login)

### Installation
1. Install QtDMM as described in [Troubleshooting → macOS](https://github.com/qtdmm/QtDMM/blob/master/docs/user/troubleshooting.md#macos)
2. Note: The app is signed ad hoc only, so first start requires *Open Anyway*

### System Information
Please record your:
- macOS version (e.g., 13.4.1)
- Mac model (e.g., MacBook Pro 14" M2 Pro)
- Architecture (Apple Silicon/Intel)
- Display type (Retina/non-Retina)
- System theme (Light/Dark)

## Test Scenarios

### 1. Installation and Startup

| Test Case | Expected Result | Actual Result | Notes |
|-----------|----------------|---------------|-------|
| First start through Gatekeeper | Can open app with "Open Anyway" or using `xattr` | | |
| Apple Silicon Mac | App launches successfully | | |
| Intel Mac | App launches successfully | | |
| Light mode | UI appears correctly with appropriate contrast | | |
| Dark mode | UI appears correctly with appropriate contrast | | |
| Retina display | Text and icons are sharp, not blurry | | |
| LCD/analog meter displays | Meter displays appear correctly with proper scaling | | |

### 2. Basic UI Interaction

| Test Case | Expected Result | Actual Result | Notes |
|-----------|----------------|---------------|-------|
| Menu access | All menus (Settings, About, Quit) are accessible | | |
| Keyboard shortcuts | Cmd shortcuts work (instead of Ctrl) | | |
| Settings dialog | Opens and functions correctly | | |
| About dialog | Shows correct version information | | |
| Quit functionality | App closes properly | | |

### 3. Device Connectivity

#### USB Serial Meters
| Test Case | Expected Result | Actual Result | Notes |
|-----------|----------------|---------------|-------|
| FTDI device | `/dev/cu.*` port appears in device list | | |
| CH340 device | `/dev/cu.*` port appears in device list | | |
| CP210x device | `/dev/cu.*` port appears in device list | | |
| PL2303 device | `/dev/cu.*` port appears in device list | | |
| Meter reading | Values are read correctly from serial port | | |

#### USB-HID Meters
| Test Case | Expected Result | Actual Result | Notes |
|-----------|----------------|---------------|-------|
| UNI-T UT-D04 style | Device is detected | | |
| WCH CH9325 | Device is detected | | |
| Hoitek | Device is detected | | |
| Meter reading | Values are read correctly | | |

#### Device Assistant
| Test Case | Expected Result | Actual Result | Notes |
|-----------|----------------|---------------|-------|
| "Add device" assistant | Opens correctly | | |
| Device detection | Finds connected cables/meters | | |
| Simulated device | Creates working simulated meter | | |

### 4. Recording and Files

| Test Case | Expected Result | Actual Result | Notes |
|-----------|----------------|---------------|-------|
| Start recording | Recording begins successfully | | |
| Stop recording | Recording stops correctly | | |
| Export as CSV | File exports correctly, can be opened in spreadsheet | | |
| Export as Excel | File exports correctly, can be opened in Excel | | |
| Export as OpenDocument | File exports correctly, can be opened in LibreOffice | | |
| Load recording | Previously saved recording loads correctly | | |
| Projects - New | New project can be created | | |
| Projects - Open | Existing project can be opened | | |
| Drag & drop files | Files can be added to project via drag & drop | | |
| Export project | Project can be exported as file | | |
| Import project | Project file can be imported | | |
| Open folder | Opens project folder in Finder | | |
| Print | Printing works correctly | | |
| Export image (PNG) | PNG exports correctly | | |
| Export image (SVG) | SVG exports correctly | | |
| Export image (PDF) | PDF exports correctly | | |
| Live in LabPlot | Works when LabPlot is installed in /Applications | | |

### 5. Network Features

| Test Case | Expected Result | Actual Result | Notes |
|-----------|----------------|---------------|-------|
| SCPI server | Can be enabled in Settings → SCPI | | |
| Bonjour discovery | Server appears via mDNS on another machine | | |
| VXI-11 connection | Can connect from another machine with NI-VISA | | |
| HiSLIP connection | Can connect from another machine with pyvisa | | |
| Firewall prompt | macOS firewall prompts when server starts | | |

### 6. Sound Features

| Test Case | Expected Result | Actual Result | Notes |
|-----------|----------------|---------------|-------|
| Alarm sounds | Limit test/alarms play correctly | | |

### 7. Update Mechanism (Development builds only)

| Test Case | Expected Result | Actual Result | Notes |
|-----------|----------------|---------------|-------|
| Update banner | Shows when update is available | | |
| Install update | Downloads new .dmg, replaces QtDMM.app, restarts | | |

## Reporting Issues

When reporting problems, please include:
1. Your system information (from above)
2. Exact steps to reproduce the issue
3. Expected vs actual behavior
4. Screenshots or error messages
5. Logs (accessible via *Log...* in the menu)

For clear, reproducible bugs, please file a separate issue with this information.

## Thank You!

Even "I opened it, clicked around for ten minutes, everything looked fine" is valuable feedback. Please report your findings in the original issue thread.