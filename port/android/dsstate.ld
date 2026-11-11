/* COFF sorts contributions after '$'; ELF needs an explicit ordering rule.
 * KEEP is necessary: callers legally access split objects across symbol ends.
 * This preserves packed runs, not yet the PC's complete save-state inventory.
 */
SECTIONS {
  .dsstate : ALIGN(4) {
    KEEP(*(SORT_BY_NAME(.dsstate$*)))
  }
}
INSERT AFTER .data;
