#ifndef TTY_H
#define TTY_H

#include <unistd.h>
#include <termios.h>
#include <stdbool.h>
#include <sys/ioctl.h>

static struct winsize winsize;
static struct termios old_termios;

// Returns pointer to static winsize struct. Use ws_col and ws_row fields.
// Single ioctl call - avoid calling get_win_cols()/get_win_rows() separately.
// Sets reasonable defaults (80x24) if ioctl fails.
static inline struct winsize *get_win_size(void)
{
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &winsize) != 0 || winsize.ws_col == 0) {
		winsize.ws_col = 80;
		winsize.ws_row = 24;
	}
	return &winsize;
}

static inline bool disable_tty_flags(tcflag_t flags)
{
	if (tcgetattr(STDIN_FILENO, &old_termios) != 0)
		return false;
	struct termios new_termios = old_termios;
	new_termios.c_lflag &= ~flags;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
	return true;
}

static inline void reset_tty(void)
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
}

#endif  // TTY_H
