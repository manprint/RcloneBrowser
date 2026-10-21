# Synthetic fixtures

Hand-written inputs for the parser's I-3 (never crash) and edge-case tests. Not recorded from rclone; the
real output is in `../rclone-1.75/`.

## panic.txt

Four lines of a Go runtime panic as printed on stderr when rclone crashes (not JSON): the `panic:` line,
the `[signal SIGSEGV ...]` line, an empty line, and `goroutine 1 [running]:`. Each line must parse as a
Text event whose msg equals the raw line.

## stats_variants.jsonl

One case per line, in this order (line N = case N):

| Line | Case | Content | Expected parse |
|---|---|---|---|
| (a) | unknown size | stats line, transferring item with `size:-1, percentage:0, eta:null` | Stats; item size -1, eta nullopt |
| (b) | over 100% | stats line, item with `bytes:150, size:100, percentage:150` | Stats; item bytes 150, percentage 150 (not clamped by the parser) |
| (c) | minimal stats | `{"level":"notice","msg":"m","stats":{"bytes":5}}` | Stats; bytes 5, everything else default |
| (d) | not an object | `[1,2]` | Text |
| (e) | object without level/msg | `{"foo":1}` | Text |
| (f) | level alias | `{"level":"warning","msg":"w"}` | Log, Notice |
| (g) | error with object | `{"level":"error","msg":"e","object":"o"}` | Log, Error, object `o` |
| (h) | truncated JSON | `{"level":"info","msg":"x` | Text |
| (i) | ANSI colors (bisync, R14) | msg `\u001b[32mBisync successful\u001b[0m`, object `\u001b[36mp\u001b[0m` | Log, Info, msg `Bisync successful`, object `p` |

## lsjson_variants.jsonl

Input for `lsjson` line parsing (`src/lsjson_parser`), one case per line, in this order (line N = case N).
The file ends with an empty line after (f), so it is `...something\n\n`.

| Line | Case | Content | Expected parse |
|---|---|---|---|
| (a) | unknown size, with a MIME type | lsjson entry `Size:-1`, trailing `,` | entry `Doc`; size -1; modTime valid |
| (b) | no ModTime | entry `x`, `Size:12`, no `ModTime` | entry `x`; size 12; modTime invalid |
| (c) | no Name | `{"Path":"y","Size":1}` | nullopt; valid JSON object, not counted as skipped |
| (d) | array open | `[` | nullopt; not counted as skipped |
| (e) | array close | `]` | nullopt; not counted as skipped |
| (f) | rclone log text on stdout | `2026/10/21 10:00:00 NOTICE: something` | nullopt; counted as 1 skipped |
| (g) | empty line | (empty) | nullopt; not counted as skipped |
