![GitHub release (latest by date)](https://img.shields.io/github/v/release/N6REJ/winkill)
![GitHub top language](https://img.shields.io/github/languages/top/N6REJ/winkill)
![GitHub All Releases](https://img.shields.io/github/downloads/N6REJ/winkill/total)
![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)

# Overview

WinKill lets you temporarily disable the Windows key on your keyboard without rebooting your computer.  
This is useful if you're always hitting the Windows key when playing games.  
Click the WinKill icon in the system tray to use it as a system-wide Windows key disabler or toggle it on/off with the "Pause"/"Break" key (default, now user-configurable).  
To exit completely, right-click on the icon and choose exit.

## New Features

- **Autostart Option:** Enable or disable automatic startup with Windows via the settings dialog.
- **Startup State:** Choose whether the application starts in "active" or "inactive" mode (tray icon reflects state).
- **User-Configurable Hotkey:** Set your own global hotkey (with modifiers like Ctrl, Alt, Shift, Win) to toggle the app, instead of the default Pause/Break key.
- **System Tray Integration:** Tray icon visually indicates active/inactive state.
- **AppVeyor CI/CD:** Automated builds and GitHub releases via AppVeyor on tagged commits to the `main` branch.

# Building

1. Install Visual Studio 2022 for C++ (the Community Edition should work fine)
2. Open `WinKill.sln`
3. Compile and run

# Screenshots

There's not much to it -- just a tray icon!

![windows key active](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/winkill/screenshots/active.png)
![windows key killed](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/winkill/screenshots/killed.png)

# End-User System Requirements

- Windows 10 or higher
- Visual C++ Redistributable (if required by your build)

# Configuration

- **Settings Dialog:**  
  Access the settings dialog from the tray icon menu to:
    - Enable/disable autostart.
    - Choose startup state (active/inactive).
    - Set or change the global hotkey.

- **Default Hotkey:**  
  The default hotkey is Pause/Break (no modifiers). You can change this in the settings dialog.

# Continuous Integration & Releases

- The project is built automatically on [AppVeyor](https://ci.appveyor.com/) using the provided `appveyor.yml`.
- On every tagged commit to the `main` branch, AppVeyor creates a GitHub release and attaches the built executable.

# How to Use

1. Run the application. The tray icon will appear.
2. Use the global hotkey (default: Pause/Break) to toggle active/inactive state.
3. Right-click the tray icon to access the settings dialog and configure startup and hotkey options.

# License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0). See the [LICENSE](LICENSE) file for details.
