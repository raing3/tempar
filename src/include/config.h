#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_VER 0x07

/**
 * Config structure.
 */
typedef struct Config {
	u8 ver;
	u8 mac_enable;
	u8 mac[6];
	u32 menu_key;
	u32 trigger_key;
	u32 screen_key;
	u8 cheat_pause;
	u32 cheat_hz;
	u32 address_format;
	u8 color_file;
	s8 cheat_file;
	u8 hijack_pspar_buttons;
	u8 cheat_status;
	u8 hbid_pbp_force;
	u32 max_text_rows;
	u16 max_cheats;
	u16 max_blocks;
	u8 auto_off;
	u32 address_start;
	u32 address_end;
	u8 swap_xo;
	u8 language_file;
}  __attribute__((__packed__)) Config;

/**
 * Colors structure.
 */
typedef struct Colors {
	/** Background color */
	u32 bgcolor;
	/** Line color */
	u32 linecolor;
	/** Selected color (red) */
	u32 color01;
	/** Selected color fade amount */
	u32 color01_to;
	/** Non-selected color (grey) */
	u32 color02;
	/** Non-selected color fade amount */
	u32 color02_to;
	/* Selected menu color (red) */
	u32 color03;
	/** Non-selected menu color (grey) */
	u32 color04;
	/** Off cheat color (red) */
	u32 color05;
	/** Always on cheat color (blue) */
	u32 color06;
	/** Normal on cheat color (green) */
	u32 color07;
	/** Multi-select folder color (green) */
	u32 color08;
	/** Single-select folder color (green) */
	u32 color09;
	/** Comments/empty code color (torquise) */
	u32 color10;
} Colors;

/**
 * Loads the configuration for a file.
 *
 * @param file Pointer to a string holding the name of the config file to load.
 * @param boot_mode The boot mode.
 * @return 0 on success, -1 if the configuration file couldn't be opened.
 */
int config_load(const char *file);

int config_save(const char *file);

/**
 * Reset the configuration to its default values.
 *
 * @param boot_mode The boot mode.
 */
void config_reset();

/**
 * Loads the colors from a file.
 *
 * @param file Pointer to a string holding the name of the color file to load.
 * @return 0 on success, -1 if the color file couldn't be opened.
 */
int color_load(const char *file);

#endif