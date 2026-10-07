# Roadmap

displayif is heading toward faster, tear-free refreshes on big panels, by
letting a drawing layer work with the panel's own memory rather than around it.

## Refresh

- A documented way to draw straight into a panel's own framebuffer: ask the
  display for a writable surface, its stride and pixel order, and learn when
  it stops being valid. That saves a full-frame copy on every present.
- A shared dirty-region contract, so a drawing layer that knows which rows
  changed can tell the display, instead of each blit working it out again.

## Ports

- Hardware runs still to do: RK043 (mimxrt eLCDIF), RT1170 DSI, and a long
  Pico DVI soak on a full panel.
- mimxrt `i80bus`: a board config in pydevices, and an optional DMA path for
  bulk transfers.

Bugs, and things you need that don't work yet, go to
[issues](https://github.com/PyDevices/displayif/issues).
