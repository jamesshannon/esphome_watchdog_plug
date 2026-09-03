#!/usr/bin/env bash
#
# plug.yaml and ha/plug-pm-1.yaml are duplicated on purpose; see the header
# comment in either file for why. This check fails if they drift apart.
#
# Comments and blank lines are ignored, so each file keeps its own header, but
# every line of actual configuration must match once the Home Assistant
# wrapper's path prefix is removed.
#
# Usage: tests/check_entrypoint_drift.sh

set -uo pipefail

cd "$(dirname "$0")/.." || exit 2

readonly PREFIX='esphome_watchdog_plug/'
readonly REPO_ENTRY='plug.yaml'
readonly HA_ENTRY='ha/plug-pm-1.yaml'

for f in "$REPO_ENTRY" "$HA_ENTRY"; do
  if [ ! -f "$f" ]; then
    echo "check_entrypoint_drift: cannot find $f" >&2
    exit 2
  fi
done

# Strip the wrapper's path prefix, then drop comments and blank lines.
normalise() {
  sed "s|${PREFIX}||g" "$1" | grep -vE '^[[:space:]]*(#|$)'
}

if delta=$(diff -u \
             --label "$REPO_ENTRY" \
             --label "$HA_ENTRY (paths normalised)" \
             <(normalise "$REPO_ENTRY") <(normalise "$HA_ENTRY")); then
  echo "check_entrypoint_drift: OK"
  exit 0
fi

cat >&2 <<EOF
check_entrypoint_drift: FAILED

$REPO_ENTRY and $HA_ENTRY have diverged. They are duplicated so the ESPHome
dashboard shows the full device config, so every change must be made in both.
Only the include paths may differ.

$delta
EOF
exit 1
