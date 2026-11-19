#!/bin/zsh
# Screenshot one page in an isolated test profile (never touches the user's preferences or data).
#   tools/shot.sh <page id> <out.png> [delay ms=8000] [size=1600x1042] [build dir=build]
# Extra env passes through (e.g. DW_* page hooks); DW_LANG=fr renders in French, DW_THEME=<id> in that theme. Kills the app after delay + 25 s in any case.
set -e
ROOT=${0:A:h:h}
PAGE=$1; OUT=$2; DELAY=${3:-8000}; SIZE=${4:-1600x1042}; BUILD=${5:-$ROOT/build}
: ${DW_PROFILE:=${TMPDIR:-/tmp}/deepwater-test-profile}
: ${DW_CACHE_DIR:=${TMPDIR:-/tmp}/deepwater-test-cache}
export DW_PROFILE DW_CACHE_DIR
BIN=$BUILD/Deepwater.app/Contents/MacOS/Deepwater
[[ -x $BIN ]] || { echo "no binary at $BIN"; exit 1; }
LANGARG=(); [[ -n $DW_LANG ]] && LANGARG=(--lang $DW_LANG); [[ -n $DW_THEME ]] && LANGARG+=(--theme $DW_THEME)
perl -e "alarm $(( DELAY / 1000 + 25 )); exec @ARGV" $BIN $LANGARG --page $PAGE --size $SIZE --screenshot $OUT --delay $DELAY
