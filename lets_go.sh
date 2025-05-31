hyprctl keyword monitor eDP-1,1024x768@60,0x0,1
hyprctl keyword decoration:screen_shader "/home/vdawg/dev/hypr-audio/tmp.frag"
# we sleep for the intro movie
sleep 1
mpvpaper ALL /home/vdawg/Videos/low_income_barony/bg.mp4  -o "--input-ipc-server=/tmp/mpv-lycris.sock --loop-playlist   --slang=en   --sid=1  --sub-border-size=1   --sub-shadow-offset=2 --sub-font-size=48    --sub-margin-y=0     --video-margin-ratio-top=0.15 --video-margin-ratio-bottom=0.15 --sub-color=\"#FDDA0D\" --no-keepaspect" &
waybar &
bun /home/vdawg/dev/hypr-audio/scripts/background/start.ts &
nwg-panel &
