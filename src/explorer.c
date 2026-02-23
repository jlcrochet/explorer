#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <error.h>
#include <signal.h>

#include "lib/stdio_helpers.h"
#include "lib/tty.h"
#include "lib/esc.h"
#include "lib/keys.h"

enum { LsColor_Count = 20 };
enum LsColor {
	LsColor_no = 0, LsColor_fi = 1, LsColor_rs = 2, LsColor_di = 3,
	LsColor_ln = 4, LsColor_mh = 5, LsColor_pi = 6, LsColor_so = 7,
	LsColor_do = 8, LsColor_bd = 9, LsColor_cd = 10, LsColor_or = 11,
	LsColor_mi = 12, LsColor_su = 13, LsColor_sg = 14, LsColor_ca = 15,
	LsColor_tw = 16, LsColor_ow = 17, LsColor_st = 18, LsColor_ex = 19,
	LsColor_Unknown = -1,
};

#define PACK(a, b) ((uint16_t)(a) | ((uint16_t)(b) << 8))

static enum LsColor lookup_ls_color(const char *s, size_t len)
{
	if (len != 2) return LsColor_Unknown;
	switch (PACK(s[0], s[1])) {
	case PACK('n','o'): return LsColor_no;
	case PACK('f','i'): return LsColor_fi;
	case PACK('r','s'): return LsColor_rs;
	case PACK('d','i'): return LsColor_di;
	case PACK('l','n'): return LsColor_ln;
	case PACK('m','h'): return LsColor_mh;
	case PACK('p','i'): return LsColor_pi;
	case PACK('s','o'): return LsColor_so;
	case PACK('d','o'): return LsColor_do;
	case PACK('b','d'): return LsColor_bd;
	case PACK('c','d'): return LsColor_cd;
	case PACK('o','r'): return LsColor_or;
	case PACK('m','i'): return LsColor_mi;
	case PACK('s','u'): return LsColor_su;
	case PACK('s','g'): return LsColor_sg;
	case PACK('c','a'): return LsColor_ca;
	case PACK('t','w'): return LsColor_tw;
	case PACK('o','w'): return LsColor_ow;
	case PACK('s','t'): return LsColor_st;
	case PACK('e','x'): return LsColor_ex;
	default:            return LsColor_Unknown;
	}
}

#undef PACK

static const int tty_flags = ECHO|ICANON;

struct file
{
	char *name;
	char *name_lower;
	size_t length;
	unsigned char type;
	bool exec;
};

static struct file *files;
static size_t files_capacity = 8, files_size;

static inline const char *file_name(const struct file *file)
{
	return file->name ? file->name : "";
}

static inline const char *file_name_lower(const struct file *file)
{
	return file->name_lower ? file->name_lower : file_name(file);
}

static int compare_files(const void *a, const void *b)
{
	const struct file *file1 = a;
	const struct file *file2 = b;
	return strcmp(file_name(file1), file_name(file2));
}

static int compare_name_to_file(const void *key, const void *elem)
{
	const char *name = key;
	const struct file *file = elem;
	return strcmp(name, file_name(file));
}

struct filtered_file
{
	uint32_t idx;
	size_t match_start;
};

static int compare_name_to_filtered(const void *key, const void *elem)
{
	const char *name = key;
	const struct filtered_file *ff = elem;
	return strcmp(name, file_name(files + ff->idx));
}

static struct filtered_file *filtered;
static size_t filtered_size;

static char search_query[256];
static char search_query_lower[256];
static size_t search_len;
static size_t search_cursor;
static bool search_open;
static bool filter_case_sensitive;
static size_t prev_search_len;

static char cwd[PATH_MAX];
static const char *home_dir;
static size_t home_len;

static size_t page, page_size, win_cols;
static volatile sig_atomic_t term_resized = 0;
static volatile sig_atomic_t terminate_signal = 0;

static size_t idx, cursor, prev_cursor;

static size_t cursor_stack[64];
static size_t cursor_stack_size;

static char ls_colors[LsColor_Count][9];

static void name_pool_reset(void)
{
	if (!files || files_size == 0)
		return;

	for (size_t i = 0; i < files_size; ++i) {
		free(files[i].name);
		free(files[i].name_lower);
		files[i].name = NULL;
		files[i].name_lower = NULL;
	}
}

static void parse_ls_colors(void)
{
	char *env_ls_colors = getenv("LS_COLORS");
	if (!env_ls_colors) return;

	char *start = env_ls_colors;
	while (*start != '\0') {
		enum LsColor c = lookup_ls_color(start, 2);
		if (c) {
			// ls_colors[*] is 9 bytes; cap scan to 8 + NUL
			sscanf(start + 3, "%8[0-9;]", ls_colors[c]);
		}

		char *next = strchr(start, ':');
		if (!next) break;
		start = next + 1;
	}
}

static void get_files(void)
{
	name_pool_reset();
	files_size = 0;

	DIR *dir = opendir(".");
	if (!dir) {
		filtered_size = 0;
		prev_search_len = 0;
		return;
	}

	struct dirent *entry;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;

		if (files_size == UINT32_MAX) {
			PUTS_ERR("Error: too many files\n");
			exit(EXIT_FAILURE);
		}

		if (files_size == files_capacity) {
			size_t old_capacity = files_capacity;
			if (files_capacity > SIZE_MAX / 2) {
				PUTS_ERR("Error: too many files\n");
				exit(EXIT_FAILURE);
			}
			files_capacity *= 2;
			struct file *new_files = realloc(files, files_capacity * sizeof(struct file));
			if (!new_files) {
				perror("realloc");
				exit(EXIT_FAILURE);
			}
			files = new_files;
			memset(files + old_capacity, 0, (files_capacity - old_capacity) * sizeof(struct file));
		}

		size_t name_len = strlen(entry->d_name);
		if (name_len == 0)
			continue;
		if (name_len == SIZE_MAX) {
			PUTS_ERR("Error: out of memory\n");
			exit(EXIT_FAILURE);
		}
		size_t one_name_len = name_len + 1;
		if (one_name_len == 0) {
			PUTS_ERR("Error: out of memory\n");
			exit(EXIT_FAILURE);
		}
		char *name = strdup(entry->d_name);
		char *name_lower = malloc(one_name_len);
		if (!name || !name_lower) {
			free(name);
			free(name_lower);
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		if (name[0] == '\0') {
			free(name);
			free(name_lower);
			continue;
		}
		for (size_t i = 0; i < name_len; ++i)
			name_lower[i] = (char)tolower((unsigned char)entry->d_name[i]);
		name_lower[name_len] = '\0';
		files[files_size] = (struct file){
			.name = name,
			.name_lower = name_lower,
			.length = name_len,
			.type = entry->d_type,
			.exec = false,
		};
		struct file *file = files + files_size;
		files_size++;

		// Some filesystems report DT_UNKNOWN; resolve type and exec bit via lstat().
		if (file->type == DT_UNKNOWN || file->type == DT_REG) {
			struct stat info;
			if (lstat(file_name(file), &info) == 0) {
				if (S_ISDIR(info.st_mode))
					file->type = DT_DIR;
				else if (S_ISREG(info.st_mode))
					file->type = DT_REG;
				else if (S_ISLNK(info.st_mode))
					file->type = DT_LNK;

				if (S_ISREG(info.st_mode))
					file->exec = info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH);
			}
		}
	}

	closedir(dir);
	qsort(files, files_size, sizeof(struct file), compare_files);

	if (files_size > 0) {
		if (files_size > SIZE_MAX / sizeof(struct filtered_file)) {
			PUTS_ERR("Error: too many files\n");
			exit(EXIT_FAILURE);
		}
		struct filtered_file *new_filtered = realloc(filtered, files_size * sizeof(struct filtered_file));
		if (!new_filtered) {
			perror("realloc");
			exit(EXIT_FAILURE);
		}
		filtered = new_filtered;
	} else {
		free(filtered);
		filtered = NULL;
	}
	filtered_size = files_size;
	for (size_t i = 0; i < files_size; ++i) {
		filtered[i].idx = (uint32_t)i;
		filtered[i].match_start = 0;
	}

	prev_search_len = 0;  // Reset incremental filter state
}

static void apply_filter(void)
{
	// Determine case sensitivity (only when query changes)
	filter_case_sensitive = false;
	for (size_t i = 0; i < search_len; ++i) {
		if (isupper((unsigned char)search_query[i])) {
			filter_case_sensitive = true;
			break;
		}
	}
	for (size_t i = 0; i < search_len; ++i)
		search_query_lower[i] = (char)tolower((unsigned char)search_query[i]);
	search_query_lower[search_len] = '\0';

	// Incremental filtering: if adding chars, filter from current matches
	bool incremental = search_len > prev_search_len && prev_search_len > 0;
	prev_search_len = search_len;

	if (search_len == 0) {
		// No filter - show all files
		filtered_size = files_size;
		for (size_t i = 0; i < files_size; ++i) {
			filtered[i].idx = (uint32_t)i;
			filtered[i].match_start = 0;
		}
	} else if (incremental) {
		// Filter from current matches (subset)
		size_t new_size = 0;
		for (size_t i = 0; i < filtered_size; ++i) {
			struct file *file = files + filtered[i].idx;
			const char *hay = filter_case_sensitive ? file_name(file) : file_name_lower(file);
			const char *needle = filter_case_sensitive ? search_query : search_query_lower;
			const char *match = strstr(hay, needle);
			if (match) {
				filtered[new_size].idx = filtered[i].idx;
				filtered[new_size].match_start = (size_t)(match - hay);
				new_size++;
			}
		}
		filtered_size = new_size;
	} else {
		// Full filter from all files
		filtered_size = 0;
		for (size_t i = 0; i < files_size; ++i) {
			const char *hay = filter_case_sensitive ? file_name(files + i) : file_name_lower(files + i);
			const char *needle = filter_case_sensitive ? search_query : search_query_lower;
			const char *match = strstr(hay, needle);
			if (match) {
				filtered[filtered_size].idx = (uint32_t)i;
				filtered[filtered_size].match_start = (size_t)(match - hay);
				filtered_size++;
			}
		}
	}

	idx = cursor = page = 0;
}

static void print_view(void);

static void clear_screen(void)
{
	PUTS_ERR(CLS);
}

static void clear_screen_and_reset(void)
{
	PUTS_ERR(SYNC_END HOME CLSB);
}

static void handle_sigwinch(int sig)
{
	(void)sig;
	term_resized = 1;
}

static void handle_terminate(int sig)
{
	terminate_signal = sig;
}

static void handle_sigtstp(int sig)
{
	(void)sig;
	clear_screen();
	reset_tty();
	signal(SIGTSTP, SIG_DFL);
	raise(SIGTSTP);
}

static void handle_sigcont(int sig)
{
	(void)sig;
	disable_tty_flags(tty_flags);
	print_view();
	signal(SIGTSTP, handle_sigtstp);
}

// Returns true if print_view() was called (full redraw), false if only selection changed
static bool move_to_previous(void)
{
	if (filtered_size == 0)
		return false;

	if (idx == 0) {
		idx = filtered_size - 1;
		size_t new_page = idx / page_size;
		cursor = idx % page_size;

		if (new_page != page) {
			page = new_page;
			print_view();
			return true;
		}
	} else {
		idx -= 1;

		if (cursor == 0) {
			cursor = page_size - 1;
			page -= 1;
			print_view();
			return true;
		} else {
			cursor -= 1;
		}
	}
	return false;
}

static bool move_to_next(void)
{
	if (filtered_size == 0)
		return false;

	if (idx == filtered_size - 1) {
		idx = cursor = 0;

		if (page > 0) {
			page = 0;
			print_view();
			return true;
		}
	} else {
		idx += 1;
		cursor += 1;

		if (cursor == page_size) {
			cursor = 0;
			page += 1;
			print_view();
			return true;
		}
	}
	return false;
}

static bool move_to_first(void)
{
	if (filtered_size == 0)
		return false;

	idx = cursor = 0;

	if (page > 0) {
		page = 0;
		print_view();
		return true;
	}
	return false;
}

static bool move_to_last(void)
{
	if (filtered_size == 0)
		return false;

	idx = filtered_size - 1;
	size_t new_page = idx / page_size;
	cursor = idx % page_size;

	if (new_page != page) {
		page = new_page;
		print_view();
		return true;
	}
	return false;
}

static bool move_page_up(void)
{
	if (filtered_size == 0)
		return false;

	if (cursor == 0) {
		if (page == 0)
			return false;
		page -= 1;
		idx = page * page_size;
		cursor = 0;
		print_view();
		return true;
	}

	idx = page * page_size;
	cursor = 0;
	return false;
}

static bool move_page_down(void)
{
	if (filtered_size == 0)
		return false;

	size_t last_page = (filtered_size - 1) / page_size;
	size_t page_end = (page < last_page) ? page_size - 1 : (filtered_size - 1) % page_size;

	if (cursor == page_end) {
		if (page >= last_page)
			return false;
		page += 1;
		page_end = (page < last_page) ? page_size - 1 : (filtered_size - 1) % page_size;
		idx = page * page_size + page_end;
		cursor = page_end;
		print_view();
		return true;
	}

	idx = page * page_size + page_end;
	cursor = page_end;
	return false;
}

static void clear_search(void)
{
	char selection[PATH_MAX] = "";

	if (filtered_size > 0)
		snprintf(selection, sizeof(selection), "%s", file_name(files + filtered[idx].idx));

	search_query[0] = '\0';
	search_len = search_cursor = 0;
	search_open = false;
	prev_search_len = 0;  // Reset incremental filter state
	apply_filter();

		if (selection[0]) {
			struct filtered_file *found = bsearch(selection, filtered, filtered_size,
				sizeof(struct filtered_file), compare_name_to_filtered);
			if (found) {
				idx = (size_t)(found - filtered);
				page = idx / page_size;
				cursor = idx % page_size;
			}
		}
}

static void search_delete_char_back(void)
{
	if (search_cursor > 0) {
		memmove(search_query + search_cursor - 1, search_query + search_cursor, search_len - search_cursor + 1);
		search_cursor--;
		search_len--;
	}
}

static void search_delete_char_forward(void)
{
	if (search_cursor < search_len) {
		memmove(search_query + search_cursor, search_query + search_cursor + 1, search_len - search_cursor);
		search_len--;
	}
}

static inline bool is_word_char(char c)
{
	return isalnum((unsigned char)c) || c == '_';
}

static void search_delete_word_back(void)
{
	if (search_cursor > 0) {
		size_t end = search_cursor;
		bool word = is_word_char(search_query[search_cursor - 1]);
		while (search_cursor > 0 && is_word_char(search_query[search_cursor - 1]) == word)
			search_cursor--;
		memmove(search_query + search_cursor, search_query + end, search_len - end + 1);
		search_len -= (end - search_cursor);
	}
}

static void search_delete_word_forward(void)
{
	if (search_cursor < search_len) {
		size_t start = search_cursor;
		bool word = is_word_char(search_query[search_cursor]);
		while (search_cursor < search_len && is_word_char(search_query[search_cursor]) == word)
			search_cursor++;
		memmove(search_query + start, search_query + search_cursor, search_len - search_cursor + 1);
		search_len -= (search_cursor - start);
		search_cursor = start;
	}
}

static void search_delete_to_start(void)
{
	if (search_cursor > 0) {
		memmove(search_query, search_query + search_cursor, search_len - search_cursor + 1);
		search_len -= search_cursor;
		search_cursor = 0;
	}
}

static void search_insert_char(int ch)
{
	if (ch >= 32 && ch < 127 && search_len < sizeof(search_query) - 1) {
		memmove(search_query + search_cursor + 1, search_query + search_cursor, search_len - search_cursor + 1);
		search_query[search_cursor++] = (char)ch;
		search_len++;
	}
}

static void search_move_word_back(void)
{
	if (search_cursor > 0) {
		bool word = is_word_char(search_query[search_cursor - 1]);
		while (search_cursor > 0 && is_word_char(search_query[search_cursor - 1]) == word)
			search_cursor--;
	}
}

static void search_move_word_forward(void)
{
	if (search_cursor < search_len) {
		bool word = is_word_char(search_query[search_cursor]);
		while (search_cursor < search_len && is_word_char(search_query[search_cursor]) == word)
			search_cursor++;
	}
}

static int remove_recursive_at(int parent_fd, const char *name)
{
	if (parent_fd < 0)
		return -1;

	struct stat st;
	if (fstatat(parent_fd, name, &st, AT_SYMLINK_NOFOLLOW) != 0)
		return -1;

	if (S_ISDIR(st.st_mode)) {
		int fd = openat(parent_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
		if (fd < 0)
			return -1;

		DIR *dir = fdopendir(fd);
		if (!dir) {
			close(fd);
			return -1;
		}

		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_name[0] == '.' &&
				(entry->d_name[1] == '\0' ||
				 (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
				continue;

			if (remove_recursive_at(dirfd(dir), entry->d_name) != 0) {
				closedir(dir);
				return -1;
			}
		}
		closedir(dir);
		return unlinkat(parent_fd, name, AT_REMOVEDIR);
	}

	return unlinkat(parent_fd, name, 0);
}

static void delete_selected(void)
{
	if (filtered_size == 0)
		return;

	struct file *selection = files + filtered[idx].idx;
	const char *selection_name = file_name(selection);

	char prompt[PATH_MAX + 64];
	snprintf(prompt, sizeof(prompt), CUP(%zu, 1) EL(0) "Delete '%s'? (y/n) ", page_size + 3, selection_name);
	PUTS_ERR(prompt);

	for (;;) {
		int ch = getchar();
		if (ch == 'y' || ch == 'Y') {
			if (remove_recursive_at(AT_FDCWD, selection_name) == 0) {
				get_files();
				apply_filter();
				if (idx >= filtered_size && filtered_size > 0)
					idx = filtered_size - 1;
				if (idx > 0) {
					page = idx / page_size;
					cursor = idx % page_size;
				} else {
					page = cursor = 0;
				}
			}
			break;
		} else if (ch == 'n' || ch == 'N' || ch == 27) {
			break;
		}
	}

	print_view();
}

static void update_cwd(void)
{
	if (!getcwd(cwd, PATH_MAX))
		cwd[0] = '\0';
}

static void enter_directory(void)
{
	if (filtered_size == 0)
		return;

	struct file *selection = files + filtered[idx].idx;
	const char *selection_name = file_name(selection);

	if (selection->type != DT_DIR) {
		// DT_UNKNOWN and symlinks can still be directories; follow stat() for navigation.
		struct stat st;
		if (stat(selection_name, &st) != 0 || !S_ISDIR(st.st_mode))
			return;
	}

	if (chdir(selection_name) != 0)
		return;

	if (cursor_stack_size < 64)
		cursor_stack[cursor_stack_size++] = idx;

	update_cwd();
	search_query[0] = '\0';
	search_len = search_cursor = 0;
	search_open = false;
	prev_search_len = 0;
	get_files();
	idx = cursor = page = 0;
	print_view();
}

static void go_to_parent(void)
{
	if (chdir("..") != 0) return;
	update_cwd();
	clear_search();
	get_files();

	if (cursor_stack_size > 0) {
		idx = cursor_stack[--cursor_stack_size];
		if (idx >= filtered_size)
			idx = filtered_size > 0 ? filtered_size - 1 : 0;
	} else {
		idx = 0;
	}

	if (idx > 0) {
		page = idx / page_size;
		cursor = idx % page_size;
	} else {
		page = cursor = 0;
	}

	print_view();
}

static size_t search_box_col;  // Column where search query starts (for cursor positioning)

static void draw_search_box(size_t path_cols)
{
	PUTS_ERR(" /");
	PUTS_ERR(search_query);
	search_box_col = path_cols + 3;  // path + " /" = path + 2, then +1 for 1-indexed
}

static void update_selection(void)
{
	if (prev_cursor != cursor) {
		// Clear old marker (row = prev_cursor + 3 for header lines)
		PRINTF_ERR(CUP(%zu, 1) " ", prev_cursor + 3);
	}
	// Set new marker
	PRINTF_ERR(CUP(%zu, 1) ">", cursor + 3);
	prev_cursor = cursor;
}

static void print_view(void)
{
	PUTS_ERR(SYNC_BEGIN HOME CSI);
	PUTS_ERR(ls_colors[LsColor_di]);
	PUTC_ERR('m');

	const char *path = cwd;
	size_t len = strlen(cwd);

	// Replace home directory with ~
	const char *display_path = path;
	size_t display_len = len;
	bool use_tilde = home_len > 0 && len >= home_len && strncmp(path, home_dir, home_len) == 0
					 && (path[home_len] == '/' || path[home_len] == '\0');
	if (use_tilde) {
		display_path = path + home_len;
		display_len = len - home_len;
	}

	size_t path_cols;
	if ((use_tilde ? 1 : 0) + display_len <= win_cols) {
		path_cols = (use_tilde ? 1 : 0) + display_len;
		if (use_tilde)
			PUTC_ERR('~');
		WRITE_ERR(display_path, display_len);
	} else {
		path_cols = win_cols;
		if (use_tilde) {
			PUTC_ERR('~');
			WRITE_ERR(display_path, win_cols - 2);
		} else {
			WRITE_ERR(display_path, win_cols - 1);
		}
		PUTS_ERR("…");
	}

	PUTS_ERR(SGR_RESET);

	if (search_open || search_len > 0)
		draw_search_box(path_cols);

	PUTS_ERR(EL(0) "\n");
	if (page > 0)
		PUTS_ERR("↑");
	PUTS_ERR(EL(0) "\n");

	size_t start = page * page_size;

	// Max length: win_cols - 2 (marker) - 1 (dir slash) - 1 (ellipsis) - 1 (terminal edge)
	size_t max_len = win_cols > 5 ? win_cols - 5 : 1;

	for (size_t i = start, j = 0; i < filtered_size && j < page_size; ++i, ++j) {
		struct file *file = files + filtered[i].idx;
		const char *name = file_name(file);
		size_t match_start = filtered[i].match_start;
		size_t match_end = match_start + search_len;
		size_t name_len = file->length;
		bool is_dir = file->type == DT_DIR;
		bool truncated = name_len > max_len;

		enum LsColor c;

		switch (file->type) {
			case DT_BLK:
				c = LsColor_bd;
				break;
			case DT_CHR:
				c = LsColor_cd;
				break;
			case DT_DIR:
				c = LsColor_di;
				break;
			case DT_FIFO:
				c = LsColor_pi;
				break;
			case DT_LNK:
				c = LsColor_ln;
				break;
			case DT_REG:
				c = file->exec ? LsColor_ex : LsColor_fi;
				break;
			case DT_SOCK:
				c = LsColor_so;
				break;
			default:
				c = LsColor_fi;
				break;
		}

		// Draw selection marker
		PUTS_ERR(j == cursor ? "> " : "  ");
		PUTS_ERR(CSI);
		PUTS_ERR(ls_colors[c]);
		PUTC_ERR('m');

		if (search_len > 0) {
			if (truncated) {
				if (match_start >= max_len) {
					// Match entirely in overflow
					WRITE_ERR(name, max_len);
					PUTS_ERR(SGR_UNDERSCORE_ON "…" SGR_UNDERLINE_OFF);
				} else if (match_end > max_len) {
					// Match partially in overflow
					WRITE_ERR(name, match_start);
					PUTS_ERR(SGR_UNDERSCORE_ON);
					WRITE_ERR(name + match_start, max_len - match_start);
					PUTS_ERR("…" SGR_UNDERLINE_OFF);
				} else {
					// Match fully visible
					WRITE_ERR(name, match_start);
					PUTS_ERR(SGR_UNDERSCORE_ON);
					WRITE_ERR(name + match_start, search_len);
					PUTS_ERR(SGR_UNDERLINE_OFF);
					WRITE_ERR(name + match_end, max_len - match_end);
					PUTS_ERR("…");
				}
			} else {
				WRITE_ERR(name, match_start);
				PUTS_ERR(SGR_UNDERSCORE_ON);
				WRITE_ERR(name + match_start, search_len);
				PUTS_ERR(SGR_UNDERLINE_OFF);
				PUTS_ERR(name + match_end);
			}
		} else {
			if (truncated) {
				WRITE_ERR(name, max_len);
				PUTS_ERR("…");
			} else {
				PUTS_ERR(name);
			}
		}

		PUTS_ERR(SGR_RESET);
		if (is_dir && !truncated)
			PUTC_ERR('/');
		PUTS_ERR(EL(0) "\n");
	}

	if (filtered_size > 0 && start + page_size < filtered_size)
		PUTS_ERR("↓");
	PUTS_ERR(ED(0));

	if (search_open)
		PRINTF_ERR(SHOW_CURSOR CUP(1, %zu), search_box_col + search_cursor);
	else
		PUTS_ERR(HIDE_CURSOR);

	PUTS_ERR(SYNC_END);

	prev_cursor = cursor;
}

// Escape sequence key codes
enum esc_key {
	ESC_NONE,
	ESC_UP, ESC_DOWN, ESC_LEFT, ESC_RIGHT,
	ESC_HOME, ESC_END, ESC_DELETE,
	ESC_PAGE_UP, ESC_PAGE_DOWN,
	ESC_CTRL_LEFT, ESC_CTRL_RIGHT, ESC_CTRL_DELETE,
	ESC_DOUBLE  // Double escape
};

static enum esc_key read_escape_sequence(void)
{
	int next = getchar();
	if (next == EOF)
		return ESC_NONE;
	if (next == K_ESC)
		return ESC_DOUBLE;
	if (next != '[' && next != 'O')
		return ESC_NONE;

	int code = getchar();
	if (code == EOF) return ESC_NONE;
	switch (code) {
		case 'A': return ESC_UP;
		case 'B': return ESC_DOWN;
		case 'C': return ESC_RIGHT;
		case 'D': return ESC_LEFT;
		case 'H': return ESC_HOME;
		case 'F': return ESC_END;
		case '1': {
			int sub = getchar();
			if (sub == EOF) return ESC_NONE;
			if (sub == '~') return ESC_HOME;
			if (sub == ';') {
				if (getchar() == EOF) return ESC_NONE;  // consume '5'
				int dir = getchar();
				if (dir == EOF) return ESC_NONE;
				if (dir == 'C') return ESC_CTRL_RIGHT;
				if (dir == 'D') return ESC_CTRL_LEFT;
			}
			return ESC_NONE;
		}
		case '3': {
			int sub = getchar();
			if (sub == EOF) return ESC_NONE;
			if (sub == '~') return ESC_DELETE;
			if (sub == ';') {
				if (getchar() == EOF) return ESC_NONE;  // consume '5'
				if (getchar() == EOF) return ESC_NONE;  // consume '~'
				return ESC_CTRL_DELETE;
			}
			return ESC_NONE;
		}
		case '4':
			if (getchar() == EOF) return ESC_NONE;  // consume '~'
			return ESC_END;
		case '5':
			if (getchar() == EOF) return ESC_NONE;  // consume '~'
			return ESC_PAGE_UP;
		case '6':
			if (getchar() == EOF) return ESC_NONE;  // consume '~'
			return ESC_PAGE_DOWN;
	}
	return ESC_NONE;
}

int main(int argc, char **argv)
{
	struct option options[] = {
		{ "start", required_argument, 0, 's' },
		{ "help", no_argument, 0, 'h' },
		{ 0 }
	};

	char *start = NULL;
	int c;

	while ((c = getopt_long(argc, argv, "s:h", options, NULL)) != -1) {
		switch (c) {
			case '?':
				break;
			case 's':
				start = optarg;
				break;
			case 'h':
				PUTS(
					"Usage: explorer [OPTIONS] [DIR]\n"
					"\n"
					"Terminal file browser with vim-like navigation.\n"
					"\n"
					"Options:\n"
					"  -s, --start NAME    Start with the cursor on the file with the given name\n"
					"  -h, --help          Print this help\n"
					"\n"
					"Keybindings:\n"
					"  Navigation:\n"
					"    Up/Down           Move cursor up/down\n"
					"    Left/Right        Go to parent directory / Enter directory\n"
					"    Home, g           Go to first item\n"
					"    End, G            Go to last item\n"
					"    Page Up, u        Move cursor to top of page (then previous page)\n"
					"    Page Down, d      Move cursor to bottom of page (then next page)\n"
					"\n"
					"  Search:\n"
					"    /                 Open search box (filters files by substring)\n"
					"    Enter             Close search box, keep filter\n"
					"    Escape Escape     Clear search and close search box\n"
					"\n"
					"  Actions:\n"
					"    Enter             Select current file and exit\n"
					"    e                 Open file in $EDITOR\n"
					"    D, Delete         Delete file/directory (with confirmation)\n"
					"    q                 Quit without selection\n"
					"\n"
					"Output:\n"
					"  Prints the absolute path of the selected file to stdout.\n"
				);
				return EXIT_SUCCESS;
		}
	}

	if (optind + 1 < argc) {
		PUTS_ERR("Usage: explorer [OPTIONS] [DIR]\n");
		return EXIT_FAILURE;
	}

	if (argv[optind] && chdir(argv[optind]) != 0) {
		perror(argv[optind]);
		return EXIT_FAILURE;
	}

	struct winsize *ws = get_win_size();

	page_size = ws->ws_row > 3 ? ws->ws_row - 3 : 1;
	win_cols = ws->ws_col;
	page = 0;

	home_dir = getenv("HOME");
	home_len = home_dir ? strlen(home_dir) : 0;

	update_cwd();

	files = calloc(files_capacity, sizeof(struct file));
	if (!files) {
		perror("calloc");
		return EXIT_FAILURE;
	}
	get_files();

	if (start) {
		struct file *found = bsearch(start, files, files_size, sizeof(struct file), compare_name_to_file);
		if (found) {
			idx = (size_t)(found - files);
			page = idx / page_size;
			cursor = idx % page_size;
		}
	}

	parse_ls_colors();

	struct sigaction sa = {
		.sa_handler = handle_terminate,
	};
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	signal(SIGTSTP, handle_sigtstp);
	signal(SIGCONT, handle_sigcont);
	signal(SIGWINCH, handle_sigwinch);

	disable_tty_flags(tty_flags);
	atexit(reset_tty);
	atexit(clear_screen_and_reset);

	print_view();

	for (;;) {
		if (terminate_signal)
			return 128 + terminate_signal;

		int ch = getchar();
		if (ch == EOF) {
			if (errno == EINTR) {
				clearerr(stdin);
				continue;
			}
			return terminate_signal ? 128 + terminate_signal : EXIT_SUCCESS;
		}

		if (term_resized) {
			term_resized = 0;
			struct winsize *ws = get_win_size();
			page_size = ws->ws_row > 3 ? ws->ws_row - 3 : 1;
			win_cols = ws->ws_col;
			// Adjust cursor/page if they're now out of bounds
			if (filtered_size > 0) {
				if (idx >= filtered_size)
					idx = filtered_size - 1;
				page = idx / page_size;
				cursor = idx % page_size;
			}
			print_view();
		}

		if (search_open) {
			switch (ch) {
				case K_CTRL_U:
					search_delete_to_start();
					apply_filter();
					print_view();
					break;

				case K_CTRL_W:
					search_delete_word_back();
					apply_filter();
					print_view();
					break;

				case K_ESC:
					switch (read_escape_sequence()) {
						case ESC_DOUBLE:
							clear_search();
							print_view();
							break;
						case ESC_LEFT:
							if (search_cursor > 0) {
								search_cursor--;
								print_view();
							}
							break;
						case ESC_RIGHT:
							if (search_cursor < search_len) {
								search_cursor++;
								print_view();
							}
							break;
						case ESC_HOME:
							search_cursor = 0;
							print_view();
							break;
						case ESC_END:
							search_cursor = search_len;
							print_view();
							break;
						case ESC_DELETE:
							search_delete_char_forward();
							apply_filter();
							print_view();
							break;
						case ESC_CTRL_DELETE:
							search_delete_word_forward();
							apply_filter();
							print_view();
							break;
						case ESC_CTRL_LEFT:
							search_move_word_back();
							print_view();
							break;
						case ESC_CTRL_RIGHT:
							search_move_word_forward();
							print_view();
							break;
						default:
							break;
					}
					break;

				case '\n':
					search_open = false;
					print_view();
					break;

				case K_DEL:
					if (search_len == 0) {
						search_open = false;
					} else {
						search_delete_char_back();
						apply_filter();
					}
					print_view();
					break;

				case K_CTRL_H:
					search_delete_word_back();
					apply_filter();
					print_view();
					break;

				default:
					search_insert_char(ch);
					apply_filter();
					print_view();
					break;
			}
			continue;
		}

		switch (ch) {
			case '\n':
				if (filtered_size > 0) {
					struct file *selection = files + filtered[idx].idx;
					PUTS(cwd);
					PUTC('/');
					PUTS(file_name(selection));
				}
				return EXIT_SUCCESS;

			case K_ESC:
				switch (read_escape_sequence()) {
					case ESC_DOUBLE:
						if (search_len > 0) {
							clear_search();
							print_view();
						}
						break;
					case ESC_UP:    if (!move_to_previous()) update_selection(); break;
					case ESC_DOWN:  if (!move_to_next()) update_selection(); break;
					case ESC_HOME:  if (!move_to_first()) update_selection(); break;
					case ESC_END:   if (!move_to_last()) update_selection(); break;
					case ESC_DELETE: delete_selected(); break;
					case ESC_PAGE_UP:   if (!move_page_up()) update_selection(); break;
					case ESC_PAGE_DOWN: if (!move_page_down()) update_selection(); break;
					case ESC_RIGHT: enter_directory(); break;
					case ESC_LEFT:  go_to_parent(); break;
					default: break;
				}
				break;

			case '/':  // Open search
				search_open = true;
				print_view();
				break;

			case 'q':
				return EXIT_SUCCESS;

			case 'g': if (!move_to_first()) update_selection(); break;
			case 'G': if (!move_to_last()) update_selection(); break;
			case 'u': if (!move_page_up()) update_selection(); break;
			case 'd': if (!move_page_down()) update_selection(); break;
			case 'D': delete_selected(); break;
			case 'e':  // Open in editor
				if (filtered_size > 0) {
					char *editor = getenv("EDITOR");
					if (editor) {
						const char *selection_name = file_name(files + filtered[idx].idx);
						reset_tty();
						clear_screen();
						pid_t pid = fork();
						if (pid == 0) {
							execlp(editor, editor, selection_name, NULL);
							_exit(127);
						} else if (pid > 0) {
							int status;
							while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
							}
						}
						disable_tty_flags(tty_flags);
						print_view();
					}
				}
				break;
		}
	}
}
