## To Do List

### General
 * Fix compile warnings.
 * Build a signed OFW compatible EBOOT (... maybe one day, no real user benefit but has been a long term desire).
 * Verify compatibility/improve usability with the PS Vita.
 * Remove/rework features where they don't make sense nowadays.
   Would anyone view a guide on the PSP these days?
   Does PSID corruption serve any use now?
 * Document translation process.
 * Rewrite psp-utilities.jar to be web (javascript) based (should be largely possible).
 * Create separate repo for cheat DB with build process (via github actions) similar to what used to be done for forum
   threads to create the different cheat DBs.

### Code Engine
 * CWCheat
   * Test the 0x06 (pointer) code types (especially negative offset ones) as they may not all work properly. Tested Code Types:
     * Positive Single Level Pointer (8, 16 and 32-bit).
     * Negative Single Level Pointer (8, 16 and 32-bit).
     * Positive Multi Write Pointer (32-bit).
     * Positive Multi Level Pointer (32-bit).