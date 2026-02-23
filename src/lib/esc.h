#ifndef ESC_H
#define ESC_H

/*
 * Linux console escape and control sequences
 * Based on console_codes(4) man page
 */

/* ==========================================================================
 * Basic Escape Sequences
 * ========================================================================== */

#define ESC     "\x1b"
#define CSI     ESC "["
#define OSC     ESC "]"

/* ==========================================================================
 * Control Characters
 * ========================================================================== */

#define CTRL_NUL    "\x00"  // Ignored
#define CTRL_BEL    "\x07"  // Beeps
#define CTRL_BS     "\x08"  // Backspace one column
#define CTRL_HT     "\x09"  // Horizontal tab
#define CTRL_LF     "\x0a"  // Linefeed
#define CTRL_VT     "\x0b"  // Vertical tab (same as LF)
#define CTRL_FF     "\x0c"  // Form feed (same as LF)
#define CTRL_CR     "\x0d"  // Carriage return
#define CTRL_SO     "\x0e"  // Activate G1 character set
#define CTRL_SI     "\x0f"  // Activate G0 character set
#define CTRL_CAN    "\x18"  // Abort escape sequences
#define CTRL_SUB    "\x1a"  // Abort escape sequences
#define CTRL_ESC    "\x1b"  // Start escape sequence
#define CTRL_DEL    "\x7f"  // Ignored
#define CTRL_CSI    "\x9b"  // Equivalent to ESC [

/* ==========================================================================
 * ESC Sequences (not CSI)
 * ========================================================================== */

#define RIS     ESC "c"     // Reset
#define IND     ESC "D"     // Linefeed
#define NEL     ESC "E"     // Newline
#define HTS     ESC "H"     // Set tab stop at current column
#define RI      ESC "M"     // Reverse linefeed
#define DECID   ESC "Z"     // DEC private identification
#define DECSC   ESC "7"     // Save current state
#define DECRC   ESC "8"     // Restore saved state
#define DECPNM  ESC ">"     // Set numeric keypad mode
#define DECPAM  ESC "="     // Set application keypad mode
#define DECALN  ESC "#8"    // DEC screen alignment test (fill with E's)

// Character set selection
#define CHARSET_DEFAULT     ESC "%@"    // Select default (ISO 646/8859-1)
#define CHARSET_UTF8        ESC "%G"    // Select UTF-8
#define CHARSET_UTF8_OLD    ESC "%8"    // Select UTF-8 (obsolete)

// G0 character set definition
#define G0_DEFAULT  ESC "(B"    // G0 -> ISO 8859-1 mapping
#define G0_VT100    ESC "(0"    // G0 -> VT100 graphics mapping
#define G0_NULL     ESC "(U"    // G0 -> null mapping (straight to ROM)
#define G0_USER     ESC "(K"    // G0 -> user mapping

// G1 character set definition
#define G1_DEFAULT  ESC ")B"    // G1 -> ISO 8859-1 mapping
#define G1_VT100    ESC ")0"    // G1 -> VT100 graphics mapping
#define G1_NULL     ESC ")U"    // G1 -> null mapping (straight to ROM)
#define G1_USER     ESC ")K"    // G1 -> user mapping

// OSC (Operating System Command) sequences
#define OSC_RESET_PALETTE   OSC "R"    // Reset palette
#define OSC_SET_PALETTE     OSC "P"    // Set palette (followed by nrrggbb)

/* ==========================================================================
 * CSI Sequences - Cursor Movement
 * ========================================================================== */

// Single parameter sequences
#define ICH(N)  CSI #N "@"  // Insert N blank characters
#define CUU(N)  CSI #N "A"  // Cursor up N rows
#define CUD(N)  CSI #N "B"  // Cursor down N rows
#define CUF(N)  CSI #N "C"  // Cursor right N columns
#define CUB(N)  CSI #N "D"  // Cursor left N columns
#define CNL(N)  CSI #N "E"  // Cursor down N rows, to column 1
#define CPL(N)  CSI #N "F"  // Cursor up N rows, to column 1
#define CHA(N)  CSI #N "G"  // Cursor to column N in current row
#define VPA(N)  CSI #N "d"  // Cursor to row N, current column
#define HPA(N)  CSI #N "`"  // Cursor to column N in current row
#define HPR(N)  CSI #N "a"  // Cursor right N columns
#define VPR(N)  CSI #N "e"  // Cursor down N rows

// Two parameter sequences
#define CUP(ROW, COL)   CSI #ROW ";" #COL "H"   // Cursor to row, column (origin 1,1)
#define HVP(ROW, COL)   CSI #ROW ";" #COL "f"   // Cursor to row, column

/* ==========================================================================
 * CSI Sequences - Editing
 * ========================================================================== */

#define ED(N)   CSI #N "J"  // Erase display
#define EL(N)   CSI #N "K"  // Erase line
#define IL(N)   CSI #N "L"  // Insert N blank lines
#define DL(N)   CSI #N "M"  // Delete N lines
#define DCH(N)  CSI #N "P"  // Delete N characters
#define ECH(N)  CSI #N "X"  // Erase N characters

// ED parameter values
#define ED_TO_END           0   // Erase from cursor to end of display (default)
#define ED_TO_START         1   // Erase from start to cursor
#define ED_ALL              2   // Erase whole display
#define ED_ALL_SCROLLBACK   3   // Erase whole display including scroll-back (Linux 3.0+)

// EL parameter values
#define EL_TO_END       0   // Erase from cursor to end of line (default)
#define EL_TO_START     1   // Erase from start of line to cursor
#define EL_ALL          2   // Erase whole line

// Convenience macros for common operations
#define CLS             ED(2)       // Clear screen
#define CLSB            ED(3)       // Clear screen including scroll-back
#define HOME            CUP(1, 1)   // Cursor to home position
#define ERASE_LINE      EL(2)       // Erase entire line
#define ERASE_TO_EOL    EL(0)       // Erase to end of line
#define ERASE_TO_BOL    EL(1)       // Erase to beginning of line

/* ==========================================================================
 * CSI Sequences - Tab Control
 * ========================================================================== */

#define TBC(N)  CSI #N "g"  // Tab clear

// TBC parameter values
#define TBC_CURRENT     0   // Clear tab stop at current position
#define TBC_ALL         3   // Delete all tab stops

/* ==========================================================================
 * CSI Sequences - Mode Setting
 * ========================================================================== */

#define SM(N)   CSI #N "h"  // Set mode
#define RM(N)   CSI #N "l"  // Reset mode

// ECMA-48 Mode values
#define MODE_DECCRM     3   // Display control chars
#define MODE_DECIM      4   // Insert mode
#define MODE_LF_NL      20  // LF/NL: auto CR after LF/VT/FF

// Convenience macros for common modes
#define SET_INSERT_MODE     SM(4)
#define RESET_INSERT_MODE   RM(4)
#define SET_LF_NL_MODE      SM(20)
#define RESET_LF_NL_MODE    RM(20)

/* ==========================================================================
 * CSI Sequences - DEC Private Modes (DECSET/DECRST)
 * ========================================================================== */

// Set with ESC [ ? n h, reset with ESC [ ? n l
#define DECSET(N)   CSI "?" #N "h"
#define DECRST(N)   CSI "?" #N "l"

// DEC Private Mode values
#define DECCKM      1       // Cursor keys send ESC O prefix instead of ESC [
#define DECCOLM     3       // 80/132 column mode switch
#define DECSCNM     5       // Reverse video mode
#define DECOM       6       // Origin mode (cursor relative to scroll region)
#define DECAWM      7       // Auto wrap mode
#define DECARM      8       // Keyboard autorepeat
#define X10_MOUSE   9       // X10 mouse reporting
#define DECTCEM     25      // Cursor visible
#define X11_MOUSE   1000    // X11 mouse reporting
#define SYNC_OUTPUT 2026    // Synchronized output

// Synchronized output (prevents tearing during multi-part screen updates)
#define SYNC_BEGIN  CSI "?2026h"
#define SYNC_END    CSI "?2026l"

// Cursor visibility
#define HIDE_CURSOR     DECRST(25)
#define SHOW_CURSOR     DECSET(25)

// Reverse video
#define SET_REVERSE_VIDEO   DECSET(5)
#define RESET_REVERSE_VIDEO DECRST(5)

// Auto wrap
#define SET_AUTOWRAP    DECSET(7)
#define RESET_AUTOWRAP  DECRST(7)

// Mouse tracking
#define ENABLE_X10_MOUSE    DECSET(9)
#define DISABLE_X10_MOUSE   DECRST(9)
#define ENABLE_X11_MOUSE    DECSET(1000)
#define DISABLE_X11_MOUSE   DECRST(1000)

/* ==========================================================================
 * CSI Sequences - Device Communication
 * ========================================================================== */

#define DA(N)   CSI #N "c"  // Device attributes
#define DSR(N)  CSI #N "n"  // Device status report

// DSR parameter values
#define DSR_STATUS      5   // Device status report (answer: ESC [ 0 n)
#define DSR_CURSOR_POS  6   // Cursor position report (answer: ESC [ y ; x R)

/* ==========================================================================
 * CSI Sequences - Keyboard LEDs
 * ========================================================================== */

#define DECLL(N)    CSI #N "q"  // Set keyboard LEDs

// DECLL parameter values
#define DECLL_CLEAR_ALL     0   // Clear all LEDs
#define DECLL_SCROLL_LOCK   1   // Set Scroll Lock LED
#define DECLL_NUM_LOCK      2   // Set Num Lock LED
#define DECLL_CAPS_LOCK     3   // Set Caps Lock LED

/* ==========================================================================
 * CSI Sequences - Scrolling Region
 * ========================================================================== */

#define DECSTBM(TOP, BOTTOM)    CSI #TOP ";" #BOTTOM "r"    // Set scrolling region

/* ==========================================================================
 * CSI Sequences - Cursor Save/Restore (SCO)
 * ========================================================================== */

#define SCOSC   CSI "s"   // Save cursor location
#define SCORC   CSI "u"   // Restore cursor location

// Backwards compatibility aliases
#define SAVE_CURSOR_LOCATION    SCOSC
#define RESTORE_CURSOR_LOCATION SCORC

/* ==========================================================================
 * CSI Sequences - SGR (Select Graphic Rendition)
 * ========================================================================== */

#define SGR(N)  CSI #N "m"

// SGR Attribute codes
#define SGR_RESET           SGR(0)  // Reset all attributes
#define SGR_BOLD_ON         SGR(1)  // Set bold
#define SGR_HALF_BRIGHT_ON  SGR(2)  // Set half-bright (dim)
#define SGR_ITALIC_ON       SGR(3)  // Set italic (Linux 2.6.22+)
#define SGR_UNDERSCORE_ON   SGR(4)  // Set underscore
#define SGR_BLINK_ON        SGR(5)  // Set blink
#define SGR_REVERSE_VIDEO_ON SGR(7) // Set reverse video

// Font selection (ECMA-48)
#define SGR_PRIMARY_FONT    SGR(10) // Reset mapping, display control, toggle meta
#define SGR_ALT_FONT_1      SGR(11) // First alternate font
#define SGR_ALT_FONT_2      SGR(12) // Second alternate font (toggle meta)

// SGR Attribute off codes
#define SGR_UNDERLINE_ON        SGR(21) // Set underline (Linux 4.17+; was normal intensity before)
#define SGR_HALF_BRIGHT_OFF     SGR(22) // Set normal intensity
#define SGR_NORMAL_INTENSITY    SGR(22) // Alias for half-bright off
#define SGR_ITALIC_OFF          SGR(23) // Italic off (Linux 2.6.22+)
#define SGR_UNDERLINE_OFF       SGR(24) // Underline off
#define SGR_BLINK_OFF           SGR(25) // Blink off
#define SGR_REVERSE_VIDEO_OFF   SGR(27) // Reverse video off

// SGR Foreground colors (30-37)
#define SGR_FG_BLACK    SGR(30)
#define SGR_FG_RED      SGR(31)
#define SGR_FG_GREEN    SGR(32)
#define SGR_FG_BROWN    SGR(33)
#define SGR_FG_BLUE     SGR(34)
#define SGR_FG_MAGENTA  SGR(35)
#define SGR_FG_CYAN     SGR(36)
#define SGR_FG_WHITE    SGR(37)
#define SGR_FG_DEFAULT  SGR(39)

// SGR Foreground colors - bright (90-97)
#define SGR_FG_BRIGHT_BLACK     SGR(90)
#define SGR_FG_BRIGHT_RED       SGR(91)
#define SGR_FG_BRIGHT_GREEN     SGR(92)
#define SGR_FG_BRIGHT_BROWN     SGR(93)
#define SGR_FG_BRIGHT_BLUE      SGR(94)
#define SGR_FG_BRIGHT_MAGENTA   SGR(95)
#define SGR_FG_BRIGHT_CYAN      SGR(96)
#define SGR_FG_BRIGHT_WHITE     SGR(97)

// SGR Background colors (40-47)
#define SGR_BG_BLACK    SGR(40)
#define SGR_BG_RED      SGR(41)
#define SGR_BG_GREEN    SGR(42)
#define SGR_BG_BROWN    SGR(43)
#define SGR_BG_BLUE     SGR(44)
#define SGR_BG_MAGENTA  SGR(45)
#define SGR_BG_CYAN     SGR(46)
#define SGR_BG_WHITE    SGR(47)
#define SGR_BG_DEFAULT  SGR(49)

// SGR Background colors - bright (100-107)
#define SGR_BG_BRIGHT_BLACK     SGR(100)
#define SGR_BG_BRIGHT_RED       SGR(101)
#define SGR_BG_BRIGHT_GREEN     SGR(102)
#define SGR_BG_BRIGHT_BROWN     SGR(103)
#define SGR_BG_BRIGHT_BLUE      SGR(104)
#define SGR_BG_BRIGHT_MAGENTA   SGR(105)
#define SGR_BG_BRIGHT_CYAN      SGR(106)
#define SGR_BG_BRIGHT_WHITE     SGR(107)

/* SGR Extended colors (256-color and 24-bit)
 * Usage: SGR_FG_256(X) where X is 0-255
 *        SGR_FG_24_BIT(R, G, B) where R,G,B are 0-255
 * 256-color: 0-15 = IBGR, 16-231 = 6x6x6 cube, 232-255 = grayscale */
#define SGR_FG_256(X)           CSI "38;5;" #X "m"
#define SGR_FG_24_BIT(R, G, B)  CSI "38;2;" #R ";" #G ";" #B "m"
#define SGR_BG_256(X)           CSI "48;5;" #X "m"
#define SGR_BG_24_BIT(R, G, B)  CSI "48;2;" #R ";" #G ";" #B "m"

/* ==========================================================================
 * Linux Console Private CSI Sequences
 * (Not ECMA-48 or VT102, native to Linux console)
 * ========================================================================== */

/* Color configuration
 * Format: CSI 1 ; n ] - set underline color
 *         CSI 2 ; n ] - set dim color
 *         CSI 8 ] - make current color pair the default */
#define LINUX_SET_UNDERLINE_COLOR(N)    CSI "1;" #N "]"
#define LINUX_SET_DIM_COLOR(N)          CSI "2;" #N "]"
#define LINUX_SET_DEFAULT_COLORS        CSI "8]"

// Screen control
#define LINUX_SET_BLANK_TIMEOUT(N)  CSI "9;" #N "]"    // Screen blank timeout in minutes
#define LINUX_SET_BELL_FREQ(N)      CSI "10;" #N "]"   // Bell frequency in Hz
#define LINUX_SET_BELL_DURATION(N)  CSI "11;" #N "]"   // Bell duration in msec
#define LINUX_BRING_CONSOLE(N)      CSI "12;" #N "]"   // Bring console N to front
#define LINUX_UNBLANK               CSI "13]"          // Unblank the screen
#define LINUX_SET_VESA_POWERDOWN(N) CSI "14;" #N "]"   // VESA powerdown interval (minutes)
#define LINUX_PREV_CONSOLE          CSI "15]"          // Bring previous console to front (Linux 2.6.0+)
#define LINUX_SET_CURSOR_BLINK(N)   CSI "16;" #N "]"   // Cursor blink interval in ms (Linux 4.2+)

/* ==========================================================================
 * VT100/xterm Additional Sequences (for reference, not all Linux-supported)
 * ========================================================================== */

// Single shifts (not implemented in Linux console)
#define ESC_SS2     ESC "N"     // Single shift 2 (select G2 for next char)
#define ESC_SS3     ESC "O"     // Single shift 3 (select G3 for next char)

// String delimiters (not implemented in Linux console)
#define ESC_DCS     ESC "P"     // Device control string (ended by ST)
#define ESC_SOS     ESC "X"     // Start of string
#define ESC_PM      ESC "^"     // Privacy message (ended by ST)
#define ESC_ST      ESC "\\"    // String terminator

// G2//3 character set definition (not implemented in Linux console)
#define ESC_G2_DEFAULT  ESC "*B"
#define ESC_G2_VT100    ESC "*0"
#define ESC_G3_DEFAULT  ESC "+B"
#define ESC_G3_VT100    ESC "+0"

// xterm OSC sequences (for reference)
#define OSC_SET_TITLE(TXT)          OSC "0;" TXT CTRL_BEL   // Set icon name and window title
#define OSC_SET_ICON_NAME(TXT)      OSC "1;" TXT CTRL_BEL   // Set icon name
#define OSC_SET_WINDOW_TITLE(TXT)   OSC "2;" TXT CTRL_BEL   // Set window title

/* ==========================================================================
 * Alternate Names / Color Aliases
 * ========================================================================== */

// Yellow is often used instead of brown
#define SGR_FG_YELLOW           SGR_FG_BROWN
#define SGR_FG_BRIGHT_YELLOW    SGR_FG_BRIGHT_BROWN
#define SGR_BG_YELLOW           SGR_BG_BROWN
#define SGR_BG_BRIGHT_YELLOW    SGR_BG_BRIGHT_BROWN

// Gray aliases
#define SGR_FG_GRAY         SGR_FG_WHITE
#define SGR_FG_GREY         SGR_FG_WHITE
#define SGR_FG_DARK_GRAY    SGR_FG_BRIGHT_BLACK
#define SGR_FG_DARK_GREY    SGR_FG_BRIGHT_BLACK
#define SGR_BG_GRAY         SGR_BG_WHITE
#define SGR_BG_GREY         SGR_BG_WHITE
#define SGR_BG_DARK_GRAY    SGR_BG_BRIGHT_BLACK
#define SGR_BG_DARK_GREY    SGR_BG_BRIGHT_BLACK

// Dim alias
#define SGR_DIM_ON  SGR_HALF_BRIGHT_ON
#define SGR_DIM_OFF SGR_HALF_BRIGHT_OFF

#endif // ESC_H
