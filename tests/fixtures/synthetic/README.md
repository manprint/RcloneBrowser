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

## config_errors/

stderr of `rclone listremotes --long --json --ask-password=false --config <file>` in the default text log format
(`YYYY/MM/DD HH:MM:SS LEVEL : msg`), captured from rclone 1.75.2 against an encrypted config (R8). The timestamps
are fixed and the config path is replaced by `/FIXTURE/c.conf`. Each file is read as a whole and passed to
`classifyConfigLoadError(1, stderr)`.

| File | Case | Lines | Expected classification |
|---|---|---|---|
| `needs.txt` | encrypted config, no password | 1 CRITICAL: `unable to decrypt configuration and not allowed to ask for password` | NeedsPassword |
| `wrong.txt` | wrong `RCLONE_CONFIG_PASS` | ERROR `Couldn't decrypt configuration, most likely wrong password.`, then CRITICAL `using RCLONE_CONFIG_PASS env password, unable to decrypt configuration` | WrongPassword |
| `cmdfail.txt` | `RCLONE_PASSWORD_COMMAND` exits non-zero | ERROR `Using --password-command returned: exit status 1`, then CRITICAL `password command failed: exit status 1` | PasswordCommandFailed |
| `other.txt` | not a password problem | text form of the recorded `../rclone-1.75/config_fail.jsonl` CRITICAL message (`didn't find section in config file`) | Other |

## jobopts_v8.bin

A tasks file (`tasks.bin`) holding one JobOptions, written by `ListOfJobOptions::Persist` with the code of
classVersion 8 (QDataStream `Qt_5_2`), before Bisync added the version 9 fields (I-7). The task is `makeV8()` in
`tests/tst_job_options.cpp`: a distinctive non-default value in every version 8 field. T-JO-COMPAT loads it with
the current code and compares every field; the version 9 fields must keep their defaults. Never re-record it:
the recorder (`RB_RECORD_JO_V8=1`) asserts classVersion 8 and so only runs on that code.
sha256 `a9487951d2a8471b2f61ff346c25bc4b4cd9a136335771009dc4cb289dc47298`, 548 bytes.

## jobopts_v9.bin

A tasks file holding two JobOptions, written by `ListOfJobOptions::Persist` with the code of classVersion 9, before
13.1 added the version 10 fields (upload headers and S3 tags, I-7). The tasks are `makeV9()` (a Download Bisync
task) and `makeV9Upload()` (an Upload Copy task to a remote of type `s3`) in `tests/tst_job_options.cpp`, in that
file order: distinctive values in every version 9 field, every bisync bool non-default in one of them. T-JO-COMPAT-V9
loads it with the current code and compares every field; the version 10 fields must stay empty. Never re-record it:
the recorder (`RB_RECORD_JO_V9=1`) asserts classVersion 9 and so only runs on that code.
sha256 `f9ee49f9e4103eb4f7270074437b09f8f71537857f575faca1cf886fc6f62dfe`, 1184 bytes.
