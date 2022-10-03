#ifndef __LIBCWCHEAT_H__
#define __LIBCWCHEAT_H__

#include <pspkerneltypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocates a new cheat entry in the cheat list
 *
 * @par Example:
 * @code
 * int cheatnumber;
 * cheatnumber = add_cheat_pspcheat_prx("unlimited money", 0);
 * @endcode
 *
 * @param cheat_name - An arbitrary name assigned to the new cheat entry.
 * @param enabled - sets if the cheat must be already enabled ( 0 = disabled, 1 = enabled).
 *
 * @return number assigned to the cheat on success, -1 on error (maximum number of cheats has been reached or wrong enabled value).
 */
int add_cheat_pspcheat_prx(char cheat_name[31], char enabled);

/**
 * Add a new line of code (codeline) in a particular, already allocated, cheat entry
 *
 * @par Example:
 * @code
 * int ret;
 * ret = add_codeline_pspcheat_prx(cheatnumber, 0x20012345, 0x12345678)
 * @endcode
 *
 * @param cheatnum - number of the cheat entry were to add the codeline (return from add_cheat_pspcheat_prx).
 * @param adress - Left part of the codeline.
 * @param value - Right part of the codeline.
 * @return 0 on success, -1 on error: wrong cheatnum (eg: not existant) or limit of codelines reached
 */
int add_codeline_pspcheat_prx(int cheatnum, u32 adress, u32 value);

/**
 * Loads/reloads cwcheat configuration files
 *
 * @par Example:
 * @code
 * int ret;
 * ret = read_config();
 * @endcode
 *
 * @return 0 on success, -1 if unable to open the file
 */
int read_config(void);

/**
 * Saves current cwcheat configuration to file
 *
 * @par Example:
 * @code
 * int ret;
 * ret = config_saver();
 * @endcode
 *
 * @return 0 on success, -1 if unable to open the file
 */
int config_saver(void);

/**
 * Calls the cheat engine and executes enabled cheats
 *
 * @par Example:
 * @code
 * setcodes(0);
 * @endcode
 *
 * @param Buttons - A button mask. (check pspctrl.h for more informations)
 */
void setcodes(int Buttons);

/**
 * Load/Reloads the database
 *
 * @par Example:
 * @code
 * int ret;
 * ret = readdb("SLUS_12345", 0);
 * @endcode
 *
 * @param game_id - The identifier of the game/homebrew. This is the signature which will be searched in the database to load cheats from.
 * @param dbnum - Number of the database to be opened.
 *
 * @return always 0 also if the db couldn't be loaded.
 */
int readdb(char game_id[], char dbnum);

#ifdef __cplusplus
}
#endif

#endif
