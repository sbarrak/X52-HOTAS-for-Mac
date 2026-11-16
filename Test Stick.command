#!/bin/bash
# Double-click this file to test the X52 Pro without X-Plane.
cd "$(dirname "$0")" || exit 1
PROBE=./bin/x52probe
if [ ! -x "$PROBE" ]; then
    echo "Building the test tool first..."
    make bin/x52probe || { echo "Build failed."; read -r -p "Press Return to close."; exit 1; }
fi

while true; do
    echo
    echo "=========== X52 Pro test ==========="
    echo "  1  Name controls: move or press anything, the stick's screen"
    echo "     (and this window) shows what it is.  Ctrl-C to stop."
    echo "  2  Write a message on the stick's screen"
    echo "  3  Lights green      4  Lights amber"
    echo "  5  Lights red        6  Lights off"
    echo "  7  Clear the stick's screen"
    echo "  8  List the stick's controls"
    echo "  q  Quit"
    read -r -p "Choose: " choice
    case "$choice" in
        1) trap 'echo' INT; "$PROBE" --identify 3600; trap - INT ;;
        2) read -r -p "Line 1 (16 characters): " l1
           read -r -p "Line 2: " l2
           read -r -p "Line 3: " l3
           "$PROBE" --text "$l1" "$l2" "$l3" ;;
        3) "$PROBE" --leds green ;;
        4) "$PROBE" --leds amber ;;
        5) "$PROBE" --leds red ;;
        6) "$PROBE" --leds off ;;
        7) "$PROBE" --clear ;;
        8) "$PROBE" --watch 0 ;;
        q|Q) exit 0 ;;
        *) echo "Please type one of the numbers above, or q." ;;
    esac
done
