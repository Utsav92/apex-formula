#!/bin/bash
# 4K (3840x2160, 60 fps) render path. One live race run captured off-screen (-RenderOffScreen + r.SetRes), PNG frames encoded in 600-frame chunks as they
# appear (and deleted) so the disk never holds more than a few GB, then concatenated and muxed with Audio/export/showcase_mix.wav.
# usage: Cinematics/render_4k.sh [WxH]   (default 3840x2160). Output: Cinematics/render/APEX_FORMULA_NYC_DEMO_<tag>.mp4
set -u
RES=${1:-3840x2160}; W=${RES%x*}; H=${RES#*x}; TAG=${2:-4K_60FPS}
P=$HOME/Development/APEXFormula; FR=$P/Saved/Cine_$TAG; SEG=$P/Saved/Seg_$TAG; OUT=$P/Cinematics/render/APEX_FORMULA_NYC_DEMO_$TAG.mp4
CHUNK=600; TOTAL=7200
rm -rf "$FR" "$SEG"; mkdir -p "$FR" "$SEG" "$P/Cinematics/render"
LOG=$P/Cinematics/render/render_$TAG.log
perl -e 'alarm 21600; exec @ARGV' ~/unreal/UE_5.8/Engine/Binaries/Mac/UnrealEditor "$P/APEXFormula.uproject" "/Engine/Maps/Entry?game=/Script/APEXFormula.APEXFormulaGameMode" \
  -game -RenderOffScreen -ForceRes -ResX=$W -ResY=$H -windowed -nosound -unattended -benchmark -fps=60 -APEXAutoDrive -APEXNoHUD -APEXNoMenu -APEXNoIntro \
  -APEXAllowPlaceholders -APEXCinematic -APEXCinematicCapture="$FR" -ExecCmds="r.SetRes ${W}x${H}w" -stdout -FullStdOutLogOutput > "$LOG" 2>&1 &
UE=$!
encode() {  # $1 = first frame, $2 = count
  local s=$1 n=$2 i
  for ((i=s;i<s+n;i++)); do f=$(printf "$FR/frame_%05d.png" $i); [ -f "$f" ] || { echo "missing frame $i (duplicated from previous)" >> "$LOG.frames"; cp "$(printf "$FR/frame_%05d.png" $((i-1)))" "$f"; }; done
  ffmpeg -loglevel error -y -framerate 60 -start_number $s -i "$FR/frame_%05d.png" -frames:v $n -c:v libx264 -preset fast -crf 14 -pix_fmt yuv420p "$SEG/seg_$(printf %05d $s).mp4" && \
  for ((i=s;i<s+n;i++)); do rm -f "$(printf "$FR/frame_%05d.png" $i)"; done
}
next=0
while kill -0 $UE 2>/dev/null; do
  sleep 20
  have=$(ls "$FR" 2>/dev/null | wc -l | tr -d ' ')
  while [ $((next+CHUNK+5)) -le $((next+have)) ] && [ -f "$(printf "$FR/frame_%05d.png" $((next+CHUNK+2)))" ]; do encode $next $CHUNK; next=$((next+CHUNK)); have=$(ls "$FR" | wc -l | tr -d ' '); done
done
rm -f "$FR/frame_$(printf %05d $TOTAL).png"
while [ $next -lt $TOTAL ]; do n=$((TOTAL-next)); [ $n -gt $CHUNK ] && n=$CHUNK; encode $next $n; next=$((next+n)); done
ls "$SEG"/seg_*.mp4 | sed "s|^|file '|;s|$|'|" > "$SEG/list.txt"
ffmpeg -loglevel error -y -f concat -safe 0 -i "$SEG/list.txt" -i "$P/Audio/export/showcase_mix.wav" -c:v copy -c:a aac -b:a 256k -shortest "$OUT"
echo "DONE $OUT"
