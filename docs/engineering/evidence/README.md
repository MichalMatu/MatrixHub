# Commercial hardening evidence

This directory is for reviewed, sanitized closure evidence that belongs in Git.
Raw device captures, endpoint snapshots, logs, network identifiers, credentials,
and operator free text stay under the ignored `artifacts/` tree.

Use one directory per stage and clean firmware commit:

```text
docs/engineering/evidence/<stage>/<UTC-date>-<40-hex-commit>/
```

Before adding a closure summary:

1. verify the device reported the exact clean 40-hex commit,
2. keep hashes of the private inputs in the private run directory,
3. copy only the tool-generated allow-listed closure summary,
4. review the copied file for SSIDs, IP/MAC addresses, rule names/IDs, tokens,
   usernames, paths, and operator notes,
5. record the reproducing command and pass/fail policy in the stage runbook.

For G1, the authoritative private inputs are the lossless CSI capture and the
CSI-to-alarm gate JSONL. A closure is not valid without reviewed real motion and
quiet intervals, transition continuity across the required reconnect case, and
at least 3600 seconds of observation on the same clean firmware commit.
