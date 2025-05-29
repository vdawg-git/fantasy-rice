bun /home/vdawg/dev/hypr-audio/scripts/background/start.ts

hyprctl plugin load /home/vdawg/dev/hypr-audio/plugin/hypraudio.so

mpvpaper ALL /home/vdawg/Videos/low_income_barony/bg.mp4  -o "--input-ipc-server=/tmp/mpv-lycris.sock --loop-playlist   --slang=en   --sid=1  --sub-border-size=1 --sub-shadow-offset=2 --sub-font-size=48    --sub-margin-y=0       --sub-color=\"#FDDA0D\" --no-keepaspect"
