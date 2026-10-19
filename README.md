# displayif

**displayif has moved.** It now lives in
[micropython-pydevices, `modules/displayif`](https://github.com/PyDevices/micropython-pydevices/tree/main/modules/displayif), with its history.
It never published on its own: it reaches you in firmware built by
micropython-pydevices' `build_mp.py`, so that is where it is built, tested and
changed now.

Name it in a build as before: `build_mp.py ... --modules displayif` (or `all`).

Your code doesn't change: `spibus`, `i80bus`, `mipidsi`, `dotclockframebuffer` and the other bus modules import as before. Open issues and pull
requests against [micropython-pydevices](https://github.com/PyDevices/micropython-pydevices).
This repository is kept read-only as a record.
