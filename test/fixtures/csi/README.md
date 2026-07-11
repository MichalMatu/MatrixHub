# Reviewed Real CSI Fixtures

This directory intentionally contains no generated detector fixtures. Add only
captures promoted by `scripts/sensing_analysis/csi_capture.py promote` after
human ground-truth and acceptance-threshold review.

Each child directory must contain exactly:

```text
<fixture-id>/
  frames.mhcf
  scenario.json
```

Run the release corpus gate with:

```bash
python scripts/tests/run_csi_fixture_replay.py
```

An empty corpus is an explicit failure of that command. The ordinary native
suite reports the real-data case as skipped unless
`MATRIXHUB_CSI_FIXTURE_ROOT` is set.
