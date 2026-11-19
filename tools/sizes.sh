#!/bin/zsh
# Screenshot one page at the laptop/desktop size matrix (offscreen renderer, so sizes larger than this screen work).
#   tools/sizes.sh <page id> <out dir> [delay ms=9000] [build dir=build] [sizes...]
# DW_LANG=fr renders in French, DW_THEME=<id> in that theme. Sizes are window client sizes; the page area is the size minus the 76 px header. Uses an isolated DW_PROFILE.
set -e
ROOT=${0:A:h:h}
PAGE=$1; OUT=$2; DELAY=${3:-9000}; BUILD=${4:-$ROOT/build}; shift 4 2>/dev/null || shift $#
DEFAULT_SIZES=(1024x600 1280x680 1366x700 1440x820 1536x780 1600x1042 1728x1080 1920x1000 2560x1360 3440x1360)
if (( $# )); then SIZES=($@); else SIZES=($DEFAULT_SIZES); fi
: ${DW_PROFILE:=${TMPDIR:-/tmp}/deepwater-test-profile}
: ${DW_CACHE_DIR:=${TMPDIR:-/tmp}/deepwater-test-cache}
export DW_PROFILE DW_CACHE_DIR QT_QPA_PLATFORM=offscreen
mkdir -p $OUT
LANGARG=(); [[ -n $DW_LANG ]] && LANGARG=(--lang $DW_LANG); [[ -n $DW_THEME ]] && LANGARG+=(--theme $DW_THEME)
for s in $SIZES; do
  perl -e "alarm $(( DELAY / 1000 + 25 )); exec @ARGV" $BUILD/Deepwater.app/Contents/MacOS/Deepwater \
    $LANGARG --page $PAGE --size $s --screenshot $OUT/${PAGE//\//_}_$s.png --delay $DELAY >/dev/null 2>&1 || echo "failed $s"
done
