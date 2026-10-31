#!/bin/sh
# Record the real editor for the browser demo page. anvil is a
# full-screen terminal program: it needs a pty, raw-mode termios and
# ioctl(TIOCGWINSZ), none of which a browser tab provides, so the demo
# is this honest capture - the actual build/anvil binary editing a
# file inside xfce4-terminal under Xvfb, recorded with ffmpeg.
# Produces docs/demo.mp4.
set -e
cd "$(dirname "$0")/.."
OUT=docs/demo.mp4
DISP=:88
rm -f /tmp/.X88-lock /tmp/.X11-unix/X88 2>/dev/null || true

Xvfb $DISP -screen 0 900x620x24 >/dev/null 2>&1 &
XVFB=$!
sleep 1
export DISPLAY=$DISP

ffmpeg -y -loglevel error -f x11grab -video_size 900x620 -i $DISP \
    -c:v libx264 -pix_fmt yuv420p -movflags +faststart "$OUT" &
REC=$!
sleep 1

mkdir -p /tmp/anvil-demo
xfce4-terminal --geometry 100x30+40+10 \
    --command "$PWD/build/anvil /tmp/anvil-demo/hello.c" >/dev/null 2>&1 &
sleep 5

sleep 1   # no WM needed: XTEST events reach the terminal directly

# navigate to the puts line, then edit for real (XTEST events to the
# focused window - VTE ignores synthetic XSendEvent keys)
xdotool key Down Down Down
sleep 0.6
xdotool key End
sleep 0.4
xdotool key Return
sleep 0.6
xdotool type --delay 90 'printf("anvil is editing this file, live\n");'
sleep 0.8
xdotool key Return
sleep 0.6
xdotool type --delay 90 '/* saved with Ctrl-S */'
sleep 0.8
xdotool key ctrl+s          # write the file to disk
sleep 1.2
xdotool key ctrl+z          # undo the comment
sleep 0.8
xdotool key ctrl+r          # redo it (anvil: redo is Ctrl-R)
sleep 1.0
xdotool key ctrl+s
sleep 1.2
xdotool key ctrl+q          # quit back to the shell
sleep 2

kill $REC 2>/dev/null || true
wait $REC 2>/dev/null || true
kill $XVFB 2>/dev/null || true
echo "recorded $OUT"
