/* Soft-reset teardown registry for displayif host resources.
 *
 * File-static BSS survives MicroPython soft reset; the GC heap does not.
 * Modules that own DMA/IRQ/PIO/SDK handles must:
 *   1. Mirror those handles in file-static state
 *   2. Register a teardown fn via displayif_register_soft_reset()
 *   3. Call the same teardown from deinit/__del__/idempotent ctors
 *
 * displayif_soft_reset_all() is invoked from a usermod --wrap of mp_deinit
 * (see ports/common/soft_reset.c) so it runs even when no Python refs remain.
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

#ifdef __cplusplus
}
#endif

#endif /* DISPLAYIF_SOFT_RESET_H */
