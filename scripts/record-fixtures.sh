#!/usr/bin/env bash
# Record real rclone output as test fixtures (offline, throw-away temp dir, empty temp config).
# Usage: scripts/record-fixtures.sh <rclone> <outdir>
# Normally run through: scripts/dev.sh fixtures [qt64|qt610]  (container, --network none, non-root).
# Never touches any user rclone configuration: every run passes --config <temp file>.
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
  echo "record-fixtures: refusing to run as root (the permission-denied scenario needs a normal user)" >&2
  exit 5
fi
if [ $# -ne 2 ]; then
  echo "Usage: $0 <rclone> <outdir>" >&2
  exit 64
fi
RCLONE=$1
OUT=$2
if [ ! -x "$RCLONE" ]; then
  echo "record-fixtures: rclone not executable: $RCLONE" >&2
  exit 2
fi

# The recording must not depend on, or leak, any environment configuration.
for v in $(compgen -e | grep '^RCLONE_' || true); do unset "$v"; done

W=$(mktemp -d)
RCD_PID=""
cleanup() {
  if [ -n "$RCD_PID" ]; then kill "$RCD_PID" 2>/dev/null || true; fi
  chmod 755 "$W/ro" 2>/dev/null || true
  rm -rf "$W"
}
trap cleanup EXIT

mkdir -p "$OUT"
# Remove stale outputs before recording.
rm -f "$OUT"/*.jsonl "$OUT"/*.json "$OUT"/*.exit "$OUT"/*.stdout "$OUT"/*.stderr "$OUT"/README.md

# --- Test tree -------------------------------------------------------------
mkdir -p "$W/src" "$W/dst"
for i in 1 2 3; do head -c 1500000 /dev/urandom > "$W/src/file $i.bin"; done
mkdir -p "$W/src/dir [x] (y)"
printf 'hi' > "$W/src/dir [x] (y)/ñ\"quote'.txt"
mkdir -p "$W/src/12345 "
printf 't' > "$W/src/12345 /trail.txt"
printf 'c' > "$W/src/a:b.txt"

: > "$W/empty.conf"
printf '[mem]\ntype = memory\n\n[loc]\ntype = local\ndescription = test local\n' > "$W/remotes.conf"
# I-9 safety net (the scenarios also pass --config explicitly): the default config is an empty temp file.
export RCLONE_CONFIG="$W/empty.conf"

C="--use-json-log -v --stats 1s --stats-log-level NOTICE --stats-file-name-length 0 --color NEVER --config $W/empty.conf"

# run_jsonl <name> <rclone args...>: combined stdout+stderr -> <name>.jsonl, exit code -> <name>.exit.
run_jsonl() {
  local name=$1 rc=0
  shift
  "$RCLONE" "$@" > "$OUT/$name.jsonl" 2>&1 || rc=$?
  echo "$rc" > "$OUT/$name.exit"
}

# shellcheck disable=SC2086  # $C is deliberately word-split
run_jsonl copy_ok copy "$W/src" "$W/dst" $C --bwlimit 1M --transfers 2
# shellcheck disable=SC2086
run_jsonl sync_checks sync "$W/src" "$W/dst" $C --checksum

mkdir "$W/ro"
chmod 555 "$W/ro"
# shellcheck disable=SC2086
run_jsonl copy_errors copy "$W/src" "$W/ro/x" $C --retries 1 --low-level-retries 1
chmod 755 "$W/ro"

# shellcheck disable=SC2086
run_jsonl config_fail copy nope: "$W/dst" $C

# Stdout-only scenarios (stderr discarded).
"$RCLONE" lsjson "$W/src" --config "$W/empty.conf" > "$OUT/lsjson_tree.json" 2>/dev/null
echo 0 > "$OUT/lsjson_tree.exit"
"$RCLONE" lsjson -R --files-only "$W/src" --config "$W/empty.conf" > "$OUT/lsjson_recursive.json" 2>/dev/null
echo 0 > "$OUT/lsjson_recursive.exit"

rc=0
"$RCLONE" lsjson "$W/src/nope" --config "$W/empty.conf" \
  > "$OUT/lsjson_missing.stdout" 2> "$OUT/lsjson_missing.stderr" || rc=$?
echo "$rc" > "$OUT/lsjson_missing.exit"

"$RCLONE" listremotes --long --json --config "$W/remotes.conf" > "$OUT/listremotes.json" 2>/dev/null
echo 0 > "$OUT/listremotes.exit"

# rcd: start in the background, let it log for 1.5 s, then kill it.
RCLONE_RC_USER=u RCLONE_RC_PASS=p "$RCLONE" rcd --rc-addr 127.0.0.1:0 --use-json-log \
  --config "$W/empty.conf" > /dev/null 2> "$OUT/rcd_start.stderr" &
RCD_PID=$!
sleep 1.5
kill "$RCD_PID" 2>/dev/null || true
wait "$RCD_PID" 2>/dev/null || true
RCD_PID=""

# --- README ----------------------------------------------------------------
{
  echo "# rclone fixtures"
  echo
  echo "Recorded by \`scripts/record-fixtures.sh\` (re-record with \`scripts/dev.sh fixtures\`). Do not edit by hand."
  echo
  echo "- rclone: $("$RCLONE" version | head -n1)"
  echo "- date: $(date -u +%Y-%m-%d)"
  echo "- temp dir replaced by \`/FIXTURE\` in every file"
  echo "- \`C=$C\`"
  echo
  echo "Test tree under \`/FIXTURE/src\`: \`file 1.bin\`, \`file 2.bin\`, \`file 3.bin\` (1500000 random bytes each),"
  echo "\`dir [x] (y)/ñ\"quote'.txt\`, \`12345 /trail.txt\`, \`a:b.txt\`."
  echo
  echo "| File | Command |"
  echo "|---|---|"
  echo "| \`copy_ok.jsonl\`, \`copy_ok.exit\` | \`rclone copy src dst \$C --bwlimit 1M --transfers 2\` (stdout+stderr) |"
  echo "| \`sync_checks.jsonl\`, \`sync_checks.exit\` | \`rclone sync src dst \$C --checksum\` (after copy_ok) |"
  echo "| \`copy_errors.jsonl\`, \`copy_errors.exit\` | \`rclone copy src ro/x \$C --retries 1 --low-level-retries 1\` (\`ro\` is mode 555) |"
  echo "| \`config_fail.jsonl\`, \`config_fail.exit\` | \`rclone copy nope: dst \$C\` |"
  echo "| \`lsjson_tree.json\`, \`lsjson_tree.exit\` | \`rclone lsjson src --config empty.conf\` (stdout) |"
  echo "| \`lsjson_recursive.json\`, \`lsjson_recursive.exit\` | \`rclone lsjson -R --files-only src --config empty.conf\` (stdout) |"
  echo "| \`lsjson_missing.stdout\`, \`.stderr\`, \`.exit\` | \`rclone lsjson src/nope --config empty.conf\` |"
  echo "| \`listremotes.json\`, \`listremotes.exit\` | \`rclone listremotes --long --json --config remotes.conf\` (stdout) |"
  echo "| \`rcd_start.stderr\` | \`RCLONE_RC_USER=u RCLONE_RC_PASS=p rclone rcd --rc-addr 127.0.0.1:0 --use-json-log --config empty.conf\`, killed after 1.5 s (stderr) |"
} > "$OUT/README.md"

# --- Replace the temp path in every output ---------------------------------
ESC=$(printf '%s' "$W" | sed 's/[][\.*^$#/]/\\&/g')
for f in "$OUT"/*; do
  sed -i "s#$ESC#/FIXTURE#g" "$f"
done

echo "record-fixtures: wrote $(find "$OUT" -type f | wc -l) files to $OUT"
