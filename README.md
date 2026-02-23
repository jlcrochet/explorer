# explorer

Terminal file browser with vim-like navigation and LS_COLORS support. Prints the absolute path of the selected file to stdout.

## Build

```
make
make install
```

Installs to `~/.local/bin/`.

## Usage

```
explorer [OPTIONS] [DIR]
```

If no directory is given, the current working directory is used.

### Options

- `-s, --start NAME` -- Start with the cursor on the file with the given name.
- `-h, --help` -- Print help.

## Keybindings

### Navigation

| Key              | Action                                          |
|------------------|-------------------------------------------------|
| Up               | Move cursor up                                  |
| Down             | Move cursor down                                |
| Left             | Go to parent directory                          |
| Right            | Enter directory                                  |
| Home, g          | Go to first item                                |
| End, G           | Go to last item                                 |
| Page Up, u       | Move cursor to top of page, then previous page  |
| Page Down, d     | Move cursor to bottom of page, then next page   |

### Search

| Key              | Action                                          |
|------------------|-------------------------------------------------|
| /                | Open search box (filters files by substring)    |
| Enter            | Close search box, keep filter                   |
| Escape Escape    | Clear search and close search box               |
| Backspace        | Delete character (closes search if empty)       |
| Ctrl-U           | Delete to start of query                        |
| Ctrl-W, Ctrl-H   | Delete word back                                |
| Ctrl-Left/Right  | Move cursor by word                             |

Search uses smart case: case-insensitive by default, case-sensitive when the query contains uppercase characters.

### Actions

| Key              | Action                                          |
|------------------|-------------------------------------------------|
| Enter            | Select current file and exit                    |
| e                | Open file in `$EDITOR`                          |
| D, Delete        | Delete file or directory (with confirmation)    |
| q                | Quit without selection                          |

## Output

Prints the absolute path of the selected file to stdout. UI is rendered to stderr, so output can be piped or captured.

## Dependencies

None beyond a standard C library and POSIX environment.
