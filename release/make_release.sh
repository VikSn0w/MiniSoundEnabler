#!/bin/sh
#
#  make_release.sh  —  build the MacMini Audio Fix release
#
#  Usage: release/make_release.sh
#
#  Builds the shipping extension (stage 3, with the Mac-mini-only
#  check) and AudioProbe, runs the host tests, and writes to
#  release/dist/:
#
#    MacMiniAudioFix-3.0.img       Disk Copy 4.2 image (dImg/dCpy):
#                                    MacMini Audio Fix
#                                    Read Me               (SimpleText)
#                                    Extras:AudioProbe
#    MacMiniAudioFix-3.0.img.bin   the image as MacBinary
#    MacMiniAudioFix-3.0.img.hqx   the image as BinHex
#    MacMiniAudioFix-3.0-source.zip  the source (git HEAD)
#    SHA256SUMS
#
#  The working tree must be clean, so the source zip matches the build.

set -e

VERSION=3.0
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
RETRO68=${RETRO68:-$HOME/Retro68-toolchain}
H="$RETRO68/bin"
DIST="$HERE/dist"
NAME="MacMiniAudioFix-$VERSION"
VOL="MacMini Audio Fix $VERSION"

if [ -n "$(git -C "$ROOT" status --porcelain)" ] && [ -z "$ALLOW_DIRTY" ]; then
    echo "Uncommitted changes; commit first (or ALLOW_DIRTY=1 for a trial)." >&2
    exit 1
fi

echo "== tests"
make -s -C "$ROOT/tests" test

echo "== build"
"$ROOT/Retro68/build.sh" 3 | tail -1
cmake --build "$ROOT/AudioProbe/build" | tail -1
EXT="$ROOT/Retro68/build/stage3/MacMini Audio Fix.bin"
PROBE="$ROOT/AudioProbe/build/AudioProbe.bin"

rm -rf "$DIST"
mkdir -p "$DIST"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"; "$H/humount" >/dev/null 2>&1 || true' EXIT

echo "== disk image"
# Read Me: Mac line endings, Mac Roman
iconv -f UTF-8 -t MACINTOSH "$HERE/ReadMe.txt" | tr '\n' '\r' > "$WORK/ReadMe"
dd if=/dev/zero of="$WORK/vol.hfs" bs=1024 count=1440 2>/dev/null
"$H/hformat" -l "$VOL" "$WORK/vol.hfs" >/dev/null
"$H/hmount" "$WORK/vol.hfs" >/dev/null
"$H/hcopy" -m "$EXT" ":MacMini Audio Fix"
"$H/hcopy" -r "$WORK/ReadMe" ":Read Me"
"$H/hattrib" -t TEXT -c ttxt ":Read Me"
"$H/hmkdir" ":Extras"
"$H/hcopy" -m "$PROBE" ":Extras:AudioProbe"
"$H/hls" -lR
"$H/humount" >/dev/null

python3 -I "$HERE/macpack.py" dc42 "$WORK/vol.hfs" "$DIST/$NAME.img" "$VOL"
python3 -I "$HERE/macpack.py" macbin "$DIST/$NAME.img" "$DIST/$NAME.img.bin" \
    "$NAME.img" dImg dCpy
python3 -I "$HERE/macpack.py" binhex "$DIST/$NAME.img" "$DIST/$NAME.img.hqx" \
    "$NAME.img" dImg dCpy

echo "== source"
git -C "$ROOT" archive --format=zip --prefix="$NAME-source/" \
    -o "$DIST/$NAME-source.zip" HEAD

cd "$DIST"
shasum -a 256 "$NAME.img" "$NAME.img.bin" "$NAME.img.hqx" "$NAME-source.zip" > SHA256SUMS
ls -l
