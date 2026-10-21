# rclone fixtures

Recorded by `scripts/record-fixtures.sh` (re-record with `scripts/dev.sh fixtures`). Do not edit by hand.

- rclone: rclone v1.75.2
- date: 2026-10-21
- temp dir replaced by `/FIXTURE` in every file
- `C=--use-json-log -v --stats 1s --stats-log-level NOTICE --stats-file-name-length 0 --color NEVER --config /FIXTURE/empty.conf`

Test tree under `/FIXTURE/src`: `file 1.bin`, `file 2.bin`, `file 3.bin` (1500000 random bytes each),
`dir [x] (y)/ñ"quote'.txt`, `12345 /trail.txt`, `a:b.txt`.

| File | Command |
|---|---|
| `copy_ok.jsonl`, `copy_ok.exit` | `rclone copy src dst $C --bwlimit 1M --transfers 2` (stdout+stderr) |
| `sync_checks.jsonl`, `sync_checks.exit` | `rclone sync src dst $C --checksum` (after copy_ok) |
| `copy_errors.jsonl`, `copy_errors.exit` | `rclone copy src ro/x $C --retries 1 --low-level-retries 1` (`ro` is mode 555) |
| `config_fail.jsonl`, `config_fail.exit` | `rclone copy nope: dst $C` |
| `lsjson_tree.json`, `lsjson_tree.exit` | `rclone lsjson src --config empty.conf` (stdout) |
| `lsjson_recursive.json`, `lsjson_recursive.exit` | `rclone lsjson -R --files-only src --config empty.conf` (stdout) |
| `lsjson_missing.stdout`, `.stderr`, `.exit` | `rclone lsjson src/nope --config empty.conf` |
| `listremotes.json`, `listremotes.exit` | `rclone listremotes --long --json --config remotes.conf` (stdout) |
| `rcd_start.stderr` | `RCLONE_RC_USER=u RCLONE_RC_PASS=p rclone rcd --rc-addr 127.0.0.1:0 --use-json-log --config empty.conf`, killed after 1.5 s (stderr) |
