#ifndef FONT_H
#define FONT_H

#define TEXT_SPECIAL_LINE 0x10
#define TEXT_CTRL_UP 0x11
#define TEXT_CTRL_DOWN 0x12
#define TEXT_CTRL_LEFT 0x13
#define TEXT_CTRL_RIGHT 0x14
#define TEXT_CTRL_CIRCLE 0x15
#define TEXT_CTRL_CROSS 0x16
#define TEXT_CTRL_TRIANGLE 0x17
#define TEXT_CTRL_SQUARE 0x18

#ifdef _FONT_acorn  
#include "../fonts/acorn.h"
#elif _FONT_lucidia 
#include "../fonts/lucidia.h"
#elif _FONT_originaldebug 
#include "../fonts/originaldebug.h"
#elif _FONT_perl 
#include "../fonts/perl.h"
#elif _FONT_sparta 
#include "../fonts/sparta.h"
#elif _FONT_font4x6 
#include "../fonts/font4x6.h" 
#elif _FONT_font8x8 
#include "../fonts/font8x8.h"
#elif _FONT_misaki
#include "../fonts/misaki.h"
#elif _FONT_cmf
#include "../fonts/cmf.h"
#elif _FONT_linux
#include "../fonts/linux.h"
#elif _FONT_acornoriginal
#include "../fonts/acornoriginal.h"
#endif

#endif