#!/bin/bash
# Double-click to install X52 Pro: builds the app if needed, puts it in /Applications and
# opens it. The app then finds X-Plane 12 and installs the plugin with one button.
cd "$(dirname "$0")" || exit 1

pause() { read -r -p "Press Return to close."; }

if [ "$(uname -m)" != "arm64" ]; then
    echo "This version is for Apple Silicon Macs (M1 and later)."; pause; exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Building needs Apple's free command line tools. A window will offer to install them;"
    echo "when it finishes, double-click Install.command again."
    xcode-select --install
    pause; exit 1
fi

echo "Building (about a minute the first time)..."
out=$(make app 2>&1) || { echo "$out" | tail -20; echo "Build failed."; pause; exit 1; }

make install-app >/dev/null || { echo "Could not copy the app to /Applications."; pause; exit 1; }
xattr -dr com.apple.quarantine "/Applications/X52 Pro.app" 2>/dev/null
open "/Applications/X52 Pro.app"
echo
echo "Installed. In the window that just opened, press Install plugin, then restart X-Plane."
sleep 3
