#!/bin/sh
# package/mac/check-app.sh - check a PsyMP3.app starts, finds its font in its
# own Resources, links FDK-AAC from inside the bundle, and quits when asked.
#
# Usage: package/mac/check-app.sh PsyMP3.app [ARCH]
#
# ARCH (arm64 or x86_64) runs that half of a universal app: x86_64 on Apple
# Silicon runs under Rosetta.
set -eu

app=$1
arch=${2:-}
exe="$app/Contents/MacOS/psymp3"
run() {
    if [ -n "$arch" ]; then arch "-$arch" "$@"; else "$@"; fi
}

plutil -p "$app/Contents/Info.plist"
lipo -archs "$exe"
run "$exe" --version

# AAC needs FDK-AAC, which must load from inside the bundle.
otool -L "$exe" | grep "@executable_path/../Frameworks/libfdk-aac" \
    || { echo "check-app.sh: FDK-AAC is not loaded from the bundle" >&2; exit 1; }

# Started on SDL's dummy drivers, through --logfile, and stopped with
# SIGTERM, which SDL turns into a quit: the program then shuts down as from
# its menu and closes the log. Killed outright, the log would lose what it
# had buffered.
log=$(mktemp)
# Started directly, not through run(): a function in the background is a
# subshell, and the SIGTERM would stop that rather than PsyMP3.
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
if [ -n "$arch" ]; then
    arch "-$arch" "$exe" --debug=font --logfile "$log" &
else
    "$exe" --debug=font --logfile "$log" &
fi
pid=$!
sleep 8
kill -TERM "$pid"
i=0
while kill -0 "$pid" 2>/dev/null && [ $i -lt 20 ]; do sleep 1; i=$((i + 1)); done
if kill -0 "$pid" 2>/dev/null; then
    kill -KILL "$pid"
    echo "check-app.sh: PsyMP3 did not quit within 20 s of SIGTERM" >&2
    exit 1
fi
cat "$log"
grep -q "PsyMP3.app/Contents/Resources/data/vera.ttf" "$log" \
    || { echo "check-app.sh: the font was not loaded from the bundle" >&2; exit 1; }
grep -q "font->isValid(): 1" "$log" \
    || { echo "check-app.sh: the font did not load" >&2; exit 1; }
rm -f "$log"
echo "check-app.sh: $app${arch:+ ($arch)} starts, finds its data and quits"
