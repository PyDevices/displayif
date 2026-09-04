# displayif unit tests

| Script | Notes |
|--------|-------|
| `test_lifecycle_api.py` | Import-only: types expose `deinit` / `__del__`. Hardware soft-reset / idempotent ctor needs a board — contract in [`docs/idempotent-lifecycle.md`](../docs/idempotent-lifecycle.md); P4 `mipidsi` + Qualia `dotclockframebuffer.DotClockFramebuffer` bring-up in [`docs/soft-reset-and-bring-up.md`](../docs/soft-reset-and-bring-up.md). |
| `jpegio/test_jpegio.py` | The `jpegio` module against its corpus under unix MicroPython: `<unix micropython> tests/jpegio/test_jpegio.py` (`--record` rewrites `frames/golden_tjpgd.json`, TJpgDec's own per-frame/scale digests). Corpus + Pillow fidelity reference beside it (`make_corpus.py`, `reference.py`, `jpegwalk.py`; frames, `reference.json` and the golden under `frames/`, see its README). |

Hardware smoke scripts live under [`tools/`](../tools/) (see `tools/README.md`).
