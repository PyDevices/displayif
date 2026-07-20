/* Soft-reset teardown registry for displayif host resources.
 *
 * File-static BSS survives MicroPython soft reset; the GC heap does not.
 * Modules that own DMA/IRQ/PIO/SDK handles must:
 *   1. Mirror those handles in file-static state
 *   2. Register a teardown fn via displayif_register_soft_reset()
 *   3. Call the same teardown from deinit/__del__/idempotent ctors
 *
 * On MCU ports, ports/common/soft_reset.c --wrap=gc_sweep_all so
 * displayif_soft_reset_all() runs before the heap is swept (every registered
 * interface: mipidsi, rgbframebuffer, i80bus, picodvi, rgbmatrix, …).
 * --wrap=mp_deinit still calls it again as an idempotent safety net.
 *
 * Ports may also implement displayif_port_pre_gc_sweep() (strong symbol) for
 * non-displayif work that must happen before the sweep — e.g. esp32 stops
 * machine.Timer so armed esp_timer callbacks cannot fire into freed heap.
 */

#ifndef DISPLAYIF_SOFT_RESET_H
#define DISPLAYIF_SOFT_RESET_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*displayif_soft_reset_fn_t)(void);

/* Register fn once (idempotent). fn must be safe if already torn down. */
void displayif_register_soft_reset(displayif_soft_reset_fn_t fn);

/* Unregister fn (optional; usually leave registered as a no-op). */
void displayif_unregister_soft_reset(displayif_soft_reset_fn_t fn);

/* Tear down all registered displayif host resources. Idempotent. */
void displayif_soft_reset_all(void);

/* Optional port hook (weak default is empty). Called from __wrap_gc_sweep_all
 * before displayif_soft_reset_all() and the real sweep. */
void displayif_port_pre_gc_sweep(void);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAYIF_SOFT_RESET_H */
