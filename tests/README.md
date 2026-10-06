# displayif unit tests

| Script | Notes |
|--------|-------|
| `test_lifecycle_api.py` | Import-only: types expose `deinit` / `__del__`. Hardware soft-reset / idempotent ctor needs a board — contract in [`docs/idempotent-lifecycle.md`](../docs/idempotent-lifecycle.md); P4 `mipidsi` + Qualia `dotclockframebuffer.DotClockFramebuffer` bring-up in [`docs/soft-reset-and-bring-up.md`](../docs/soft-reset-and-bring-up.md). |

Hardware smoke scripts live under [`tools/`](../tools/) (see `tools/README.md`).
