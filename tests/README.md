# displayif unit tests

| Script | Notes |
|--------|-------|
| `test_lifecycle_api.py` | Import-only: types expose `deinit` / `__del__`. Hardware soft-reset / idempotent ctor needs a board — contract in [`docs/IDEMPOTENT_LIFECYCLE.md`](../docs/IDEMPOTENT_LIFECYCLE.md); P4 `mipidsi` + Qualia `displayif.DotClockFramebuffer` bring-up in [`docs/SOFT_RESET_AND_BRINGUP.md`](../docs/SOFT_RESET_AND_BRINGUP.md). |

Hardware smoke scripts live under [`tools/`](../tools/) (see `tools/README.md`).
