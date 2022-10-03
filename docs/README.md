# TempAR

## Introduction

TempAR is a cheat device for the PSP which supports both the PSP Action Replay and CWCheat code types.

Where does the name come from? The site [GBAtemp](https://gbatemp.net/) once upon a team had a group of dedicated
DS cheat makers. As I became involved with the site many of them became great friends and I helped expand the focus
to also include PSP cheats. "Temp" is derived from GBAtemp while "AR" is an indicator that this implements the action
replay code types. And from that came "TempAR", the angry cheat device. :)

## Usage

### Installation

1. Copy the seplugins folder to your Memory Stick
   (if not copying over game.txt and pops.txt be sure to manually add those lines to the files).

### Hotkeys

#### Global

| Button         | Effect                                            |
|----------------|---------------------------------------------------|
| RTRIGGER+HOME  | Show cheat engine menu.                           |
| NOTE           | Toggle cheat engine between enabled and disabled. |
| SELECT+VOLDOWN | Screenshot.                                       |

### General

| Button         | Effect                       |
|----------------|------------------------------|
| CROSS          | Execute selected function.   |
| CIRCLE         | Back/exit cheat engine menu. |
| LTRIGGER       | Previous tab.                |
| RTRIGGER       | Next tab.                    |

### Memory browser

| Button         | Effect                                                         |
|----------------|----------------------------------------------------------------|
| DPAD           | Move cursor and edit value of selected address (in edit mode). |
| START          | Toggle between disassembler and browser.                       |
| SELECT         | Toggle browser between 8 and 16 bytes per line.                |
| CROSS          | Toggle edit mode.                                              |
| TRIANGLE       | Show copier menu.                                              |
| VOLUP          | Next browser.                                                  |
| VOLDOWN        | Previous browser.                                              |
| SQUARE+UP/DOWN | Scroll through memory without moving cursor position.          |
| SQUARE+ANALOG  | Scroll through memory quickly without moving cursor position.  |

### Memory disassembler (decoder)

| Button         | Effect                                                                                         |
|----------------|------------------------------------------------------------------------------------------------|
| DPAD           | Move cursor and edit value of selected address (in edit mode).                                 |
| START          | Toggle between disassembler and browser.                                                       |
| SELECT         | Toggle decoder between value and opcode view.                                                  |
| CROSS          | Toggle edit mode.                                                                              |
| TRIANGLE       | Show copier menu.                                                                              |
| NOTE           | Toggle starting and ending addresses to use by the copier when copying to new cheat/text file. |
| VOLUP          | Next disassembler.                                                                             |
| VOLDOWN        | Previous disasse.                                                                              |
| SQUARE+UP/DOWN | Scroll through memory without moving cursor position.                                          |
| SQUARE+ANALOG  | Scroll through memory quickly without moving cursor position.                                  |
| SQUARE+RIGHT   | Jump to address of the selected pointer.                                                       |
| SQUARE+LEFT    | Return to pointer.                                                                             |

## Comparison to other cheat engines

### PSP action replay

**General**

 * This cheat device does not work on OFW! (durr). However it does work on CFW which the official PSPAR does not.
 * Instead of using a complex binary file for the codes a CWCheat-style file is used for simple code entry.

**Code types**

 * Fake addresses were added for reading the pad state. This is to allow codes to be more easily ported over from
   cheat devices like CWCheat.

### NitePR / MKUlta (and similar)

**General**

 * Codes are loaded from a single CWCheat-style file instead of using a single text file for each game.
   This reduces the time taken to update the cheats and save a lot of MS space. Depending on how the MS is formatted
   and the amount of code files on the MS it can save over 20MB! (big savings in 2010 :D)
 * Real addresses are used by default for the decoder and browser as PSPAR uses real values for the address.

**Code types**

 * NitePR code type support was removed, NitePR code files can still be read but are converted on load to PSPAR format.
 * SOCOM related stuff was removed.

### CWcheat

**General**

 * A new line type _C2 has been added. Code names with this line prefix will be enabled regardless of the if the cheat
   engine is enabled or not.

## Code types

 * [CWCheat](https://github.com/raing3/psp-cheat-documentation/blob/master/cheat-devices/cwcheat.md)
 * [PSP Action Replay (PSPAR)](https://github.com/raing3/psp-cheat-documentation/blob/master/cheat-devices/pspar.md)
 * [TempAR](https://github.com/raing3/psp-cheat-documentation/blob/master/cheat-devices/tempar.md)
 * [Button activators](https://github.com/raing3/psp-cheat-documentation/blob/master/other/button-activators.md)
 
## Skinning

Most of the colors can be customised by creating a file in the `colors` similar to those that already exists. Each line
in the file represents the color to use for a different part of the UI, eg:

```
;GBAtemp colors
0xFF442F18 ;background color
0xFFFFFFFF ;line color
0xFF0072FF ;selected color
0x00000202 ;selected color fade amount
0xFFFFFFFF ;non-selector color
0x00020202 ;non-selected color fade amount
0xFF0072FF ;selected menu color
0xFFFFFFFF ;non-selected menu color
0xFF0072FF ;off cheat color
0xFFFF0000 ;always on cheat color
0xFF00FF00 ;normal on cheat color
0xFF21FF00 ;multi-select folder color
0xFF21FF00 ;single-select folder color
0xFFFFFF00 ;comment/empty code color
```

## Searcher

The searcher supports 8-bit, 16-bit and 32-bit exact and unknown searches as well as text searches and user-defined
search ranges. It has been improved from MKUltra to provide significant performance improvements.

Below is a search speed performance comparison for God of War: Ghost of Sparta at the main menu on 5.50 GEN-D3:

| Search type                         | Old searched                  | New searcher  | Difference  |
|-------------------------------------|-------------------------------|---------------|-------------|
| 32-bit exact (0x00000000)           | 1:10 for 5%, ~23:20 for 100%  | 0:54 for 100% | 25x faster  |
| Continue 32-bit exact (0x00000001)  | Skipped                       | 0:17 for 100% | Skipped     |
| 8-bit exact (0x01)                  | 2:20 for 5%, ~46:30 for 100%  | 0:11 for 100% | 253x faster |
| Continue 8-bit exact (0x01)         | Skipped                       | 0:08 for 100% | Skipped     |
| 32-bit unknown                      | 0:06 for 100%                 | 0:06 for 100% | Same        |
| Continue 32-bit unknown (different) | 5:00 for 20%, 20:00+ for 100% | 0:13 for 100% | 92x faster  |
| Continue 32-bit unknown (different) | Skipped                       | 0:01 for 100% | Skipped     |
| 8-bit unknown                       | 0:06 for 100%                 | 0:06 for 100% | Same        |
| 8-bit unknown (different)           | 5:00 for 20%, 20:00+ for 100% | 0:22 for 100% | 54x faster  |
| 8-bit unknown (different)           | Skipped                       | 0:02 for 100% | Same        |

## FAQ

Q: Can I use both CWCheat and PSPAR code types in a single code?
A: No, the cheat engine supports using only one cheat engine per code (either CWCheat, PSPAR or PSPAR Extended).
   The prefix of the last line of the code determines the cheat engine used.

Q: Can I use the `pspar_codes1.bin` file from the PSPAR website with TempAR?
A: Yes, copy the file to the same directory as the TempAR plugin and TempAR will attempt to load cheats from it.

Q: Can I use my NitePR codes with TempAR?
A: Yes, if cheats for a game can't be loaded from the CWCheat/PSPAR code file it will attempt to find a NitePR cheat
   file in both `[tempar directory]/cheats/[game id].txt` directory and `ms0:/seplugins/nitePR/[game id].txt`. The
   cheats will be converted to PSPAR format upon being loaded.

Q: Can I use my CWCheat POPS codes with TempAR?
A: Yes, to use TempAR with PS1 games you should load the `tempar_lite.prx` file.

## Thanks

Special thanks go out to the following people, without them this would not have been possible:

 * Datel: Defining a very simple to implement code type specification which offers a very complex level of functionality.
 * Davee: Giving me a simple sce function call to get the Game ID, no PARAM.SFO parsing!
 * EnHacklopedia Contributors: The very extensive and well written code type information (available at Kodewerx).
 * Haro: Numerous bug reports, suggestions and some coding help.
 * NoEffex: The corrupt PSID function as well as possibly some other code.
 * Normmatt: His great code type analyzer tool.
 * RedHate: Improving upon the NitePR code base.
 * The original NitePR Coders: SANiK, imk and telazorn are probably the most to thank for this as a large amount of
   the original code and ideas were theres from NitePR.
 * weltall: The CWCheat cheat device and all the great code types which were implemented into it... even if the
   non-standard pointer codes were a bitch to implement.
 * xist: Giving me a great name for the cheat device.
 * Everyone else who helped along the way with testing, fixes and suggestions!
 

## Useful links

 * [PSPAR.com](http://www.pspar.com/): Datel PSP action replay cheat database.
 * [CWCheat DB Editor](https://code.google.com/archive/p/cwcheatdb/downloads) (by Pasky): Modify PSPAR/CWCheat format
   database files.