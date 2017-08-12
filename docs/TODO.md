## To Do List

##### General
 * Change default button to Select and add user customizable delay.
 * Finish adding the CWCheat code types to the readme.
 * Fix compile warnings.

##### Code Engine
 * General
   * More fake addresses (PSP Model, Random Number, PSP Specific Number).

 * CWCheat
   * Test the 0x06 (pointer) code types (especially negative offset ones) as they may not all work properly. Tested Code Types:
     * Positive Single Level Pointer (8, 16 and 32-bit).
     * Negative Single Level Pointer (8, 16 and 32-bit).
     * Positive Multi Write Pointer (32-bit).
     * Positive Multi Level Pointer (32-bit).