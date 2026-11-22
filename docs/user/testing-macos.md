# Testing QtDMM on macOS

This guide provides instructions for testing QtDMM on macOS. Your feedback helps improve the application for all macOS users.

## Getting Started

### Downloading QtDMM

1. **Latest Release**: Download the `.dmg` from the [latest release page](https://github.com/qtdmm/QtDMM/releases/latest)
2. **Development Build**: Get a recent build from the [macOS workflow](https://github.com/qtdmm/QtDMM/actions/workflows/cmake-mac-platform.yml) (artifact `QtDMM-macos`, requires GitHub login)

### Installation

1. Install QtDMM as described in [Troubleshooting → macOS](troubleshooting.md#macos)
2. The app is signed ad hoc only, so the first start requires *Open Anyway*

### Reporting Your Results

When commenting on your testing, please include:
- Your macOS version (e.g., macOS 14.2)
- Your Mac type (Apple Silicon or Intel)
- What worked well
- What didn't work (with screenshots or logs)

Even brief feedback like "I opened it, clicked around for ten minutes, everything looked fine" is valuable!

## Testing Checklist

### Installation and Start

- [ ] **First Start**: Test starting through Gatekeeper (*Open Anyway* or `xattr`)
- [ ] **Universal Binary**: Test on both Apple Silicon and Intel Macs
- [ ] **Theme Support**: Test light and dark mode
- [ ] **Display Quality**: Check Retina display sharpness for text, icons, and LCD/analog designs
- [ ] **UI Elements**: Test menus, keyboard shortcuts (Cmd instead of Ctrl), *Settings*, *About*, *Quit*

### Meters and Device Connectivity

- [ ] **USB Serial**: Test with FTDI, CH340, CP210x, or PL2303 cables - check if `/dev/cu.…` ports appear and read
- [ ] **USB-HID**: Test with UNI-T UT-D04 style, WCH CH9325, or Hoitek cables - check if found and read
- [ ] **Add Device Assistant**: Test if the cable/meter is found
- [ ] **Simulated Device**: Test *Add device → Simulated* for a working meter without hardware

### Recording and File Operations

- [ ] **Recording**: Test record, stop, and export functionality (CSV, Excel, OpenDocument)
- [ ] **File Loading**: Test loading a recording again
- [ ] **Projects**: Test creating new projects, opening existing ones, adding files by drag & drop
- [ ] **Project Management**: Test export/import as file, *Open folder* in Finder
- [ ] **Printing**: Test printing and *Export image* (PNG, SVG, PDF)
- [ ] **LabPlot Integration**: Test *Live in LabPlot* with LabPlot installed in `/Applications`

### Network Functionality

- [ ] **SCPI Server**: Test Settings → SCPI server and finding it via Bonjour/mDNS from another machine
- [ ] **VXI-11 and HiSLIP**: Test with NI-VISA or pyvisa on another computer
- [ ] **Firewall Prompt**: Test the macOS firewall prompt when the server starts listening

### Sound and Alarms

- [ ] **Alarm Sounds**: Test alarm sounds for limit tests and alarms

### Update Process (Development Builds Only)

- [ ] **Update Banner**: Test the update banner and *Install* functionality
- [ ] **Auto-Update**: Test that QtDMM downloads the new `.dmg`, replaces `QtDMM.app`, and restarts

### Building from Source (Optional)

If you prefer to build from source:
1. Install official Qt 6.8 or newer (not Homebrew's)
2. Run `./compile.sh` in the repository
3. Report any build issues

## Not Available on macOS Yet

- **Bluetooth LE Meters**: Not currently supported due to CoreBluetooth hiding device addresses and missing Bluetooth permission text

## Getting Help

If you encounter issues:
1. Check the [troubleshooting guide](troubleshooting.md)
2. Include screenshots or logs (from *Log...* in the menu)
3. For clear bugs, consider creating a separate issue

Thank you for helping improve QtDMM on macOS!