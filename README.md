# ViPDF

A keyboard-driven PDF reader with Vim-style motions. Built on [MuPDF](https://mupdf.com/) for rendering and [SFML](https://www.sfml-dev.org/) for the GUI.

## Features

- **Vim-style navigation**: pan, scroll, and jump through documents without touching the mouse
- **Annotations**: highlight, underline, and strikethrough, each in five colors
- **Bookmarks**: save, delete and jump to bookmarked pages within a document
- **Fully themeable**: every UI element's color can be changed through a simple INI file
- **Custom color inversion**: recolor pages with your own light and dark colors, not just a plain invert
- **Fast**: multithreaded page rendering with a configurable page cache size

## Manual Dependencies

The following dependencies must be installed manually, using your system's package manager or by building them from source:

| Dependency                              | Version            |
| --------------------------------------- | ------------------ |
| [SFML](https://www.sfml-dev.org/)       | 3.1.0+             |
| [MuPDF](https://mupdf.com/)             | any recent version |
| [Meson](https://mesonbuild.com/)        | build system       |

## Installation

```sh
git clone https://github.com/asmit-mit/ViPDF.git
cd ViPDF

meson setup build --prefix=$HOME/.local
meson compile -C build
meson install -C build
```

Make sure `~/.local/bin` is in your `PATH`.

## Usage

```sh
vipdf path/to/document.pdf
```

Press `:` to open the command line. Press `Esc` to cancel the current action and return focus to the document view.

## Keybindings

> Keybindings are currently fixed. Custom keybindings are not supported yet.

`Ctrl` and `Shift` are written explicitly. Keys separated by spaces are pressed in sequence.

### Navigation

| Key                 | Action               |
| ------------------- | -------------------- |
| `h` / `l`           | Pan left / right     |
| `k` / `j`           | Pan up / down        |
| `u` / `d`           | Previous / next page |
| `Ctrl+u` / `Ctrl+d` | Half page up / down  |
| `g g`               | Go to first page     |
| `Shift+g`           | Go to last page      |

### View

| Key       | Action                           |
| --------- | -------------------------------- |
| `Ctrl+=`  | Zoom in                          |
| `Ctrl+-`  | Zoom out                         |
| `f`       | Fit page                         |
| `w`       | Fit width                        |
| `r`       | Rotate clockwise                 |
| `Shift+r` | Rotate counter-clockwise         |
| `i`       | Toggle recolor (color inversion) |

### Search and Selection

| Key       | Action                 |
| --------- | ---------------------- |
| `n`       | Next search result     |
| `Shift+n` | Previous search result |
| `y`       | Copy selection         |

### Annotations

| Key         | Action                                                |
| ----------- | ----------------------------------------------------- |
| `a h 1`-`5` | Highlight selection with color 1-5                    |
| `a u 1`-`5` | Underline selection with color 1-5                    |
| `a s 1`-`5` | Strikethrough selection with color 1-5                |
| `a d`       | Remove annotation from the last left-clicked position |

The five annotation colors are defined as `color_1` to `color_5` in the `[highlights]` section of your config.

### General

| Key   | Action                                 |
| ----- | -------------------------------------- |
| `:`   | Open command line                      |
| `/`   | Open search                            |
| `Esc` | Cancel and return to the document view |

## Commands

Press `:` to open the command line.

| Command           | Alias | Description                                           |
| ----------------- | ----- | ----------------------------------------------------- |
| `:open <path>`    | `:o`  | Open a document                                       |
| `:reload`         |       | Reload the current document                           |
| `:close`          |       | Close the current document                            |
| `:write [path]`   | `:w`  | Save changes. Use % as path to overwrite current file |
| `:quit`           | `:q`  | Quit ViPDF                                            |
| `:clear files`    |       | Clear recent files                                    |
| `:clear search`   |       | Clear search history                                  |
| `:clear commands` |       | Clear command history                                 |

Arguments that contain spaces must be wrapped in double quotes, for example:

```
:write "example text.pdf"
```

### Bookmarks

| Command             | Description                         |
| ------------------- | ----------------------------------- |
| `:bmark add <name>` | Save the current page as a bookmark |
| `:bmark del <id>`   | Delete a bookmark                   |
| `:bmark jump <id>`  | Jump to a bookmark                  |

`:b` can be used in place of `:bmark`, for example `:b jump <id>`.

Bookmarks are created with a name (use quotes if it contains spaces, e.g. `:bmark add "chapter 3"`), but `del` and `jump` work with a bookmark's id. You don't need to remember ids:

1. Type `:bmark jump` followed by the name, or the beginning of it.
2. Press `Tab`. If bookmark with that name exists, the command line will convert your argument to it's matching id.
3. If multiple bookmarks with the same name exists, the completion list shows matching bookmarks by name. Move through the list with `Ctrl+j` / `Ctrl+k`, which switches the argument to the highlighted bookmark's id.
4. Press `Enter` to run the command.

### Completions

Press `Tab` to open completions for the current command, if it has any. Completions are available for file paths (`:open`, `:write`) and bookmark names (`:bmark del`, `:bmark jump`).

| Key      | Action                            |
| -------- | --------------------------------- |
| `Tab`    | Show completions                  |
| `Ctrl+j` | Move down in the completion list  |
| `Ctrl+k` | Move up in the completion list    |
| `Enter`  | Select the highlighted completion |
| `Esc`    | Close the completion list         |

## Configuration

ViPDF reads its configuration from:

```
~/.config/vipdf/config.ini
```

You only need to include the values you want to change; everything else falls back to the defaults below.

### Default configuration

```ini
; Default configuration.
; Colors: #rrggbb, or #rrggbbaa (alpha value supported only for highlights).

[app]
background = 0x1d2021

[cmdline]
background = 0x3c3836
text = 0xebdfc3
textbox_highlight = 0x7daea3
highlight_text = 0x3c3836

[completions]
background = 0x282828
left_text = 0xebdfc3
right_text = 0xd5c7a2
highlight = 0x458588

[notification]
background_idle = 0x1d2021
background_hover = 0x3c3836
outline = 0x5b554d
heading_text = 0xebdfc3
msg_text = 0xebdfc3

[password_dialog]
background = 0x1d2021
outline = 0x5b554d
textbox_background = 0x282828
textbox_outline = 0x5b554d
text = 0xebdfc3
textbox_highlight = 0x7daea3
highlight_text = 0x3c3836

[statusbar]
background = 0x282828
text = 0xebdfc3

[highlights]
selection = 0xADFF2F50
search = 0xADFF2F66
search_box = 0xFFD70050
color_1 = 0xf5c211
color_2 = 0xff7800
color_3 = 0xed333b
color_4 = 0x3584e4
color_5 = 0x33d17a

[recolor]
default_recolor = true
light = 0x282828
dark = 0xebdbb2

[general]
delta_zoom = 0.1
page_cache_size = 15
thread_count = 5
```

### Option reference

| Section             | Key                                                                          | Description                                                                          |
| ------------------- | ---------------------------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| `[app]`             | `background`                                                                 | Window background behind pages                                                       |
| `[cmdline]`         | `background`, `text`, `textbox_highlight`, `highlight_text`                  | Command line colors                                                                  |
| `[completions]`     | `background`, `left_text`, `right_text`, `highlight`                         | Completion popup colors                                                              |
| `[notification]`    | `background_idle`, `background_hover`, `outline`, `heading_text`, `msg_text` | Notification colors                                                                  |
| `[password_dialog]` | `background`, `outline`, `textbox_*`, `text`, `highlight_text`               | Password prompt for encrypted PDFs                                                   |
| `[statusbar]`       | `background`, `text`                                                         | Status bar colors                                                                    |
| `[highlights]`      | `selection`, `search`, `search_box`                                          | Selection and search match colors (alpha supported)                                  |
| `[highlights]`      | `color_1` ... `color_5`                                                      | Annotation palette                                                                   |
| `[recolor]`         | `default_recolor`                                                            | Start with recolor enabled (`true` / `false`)                                        |
| `[recolor]`         | `light`, `dark`                                                              | Colors that white and black are mapped to when recolor is on                         |
| `[general]`         | `delta_zoom`                                                                 | Zoom increase/decrease per zoom action                                               |
| `[general]`         | `page_cache_size`                                                            | Number of rendered pages kept in memory (increasing this will increase memory usage) |
| `[general]`         | `thread_count`                                                               | Number of rendering threads                                                          |

## Acknowledgements

ViPDF is built on these libraries:

- [MuPDF](https://mupdf.com/)
- [SFML](https://www.sfml-dev.org/)
- [HarfBuzz](https://harfbuzz.github.io/)
- [FreeType](https://freetype.org/)
- [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp)
- [nlohmann/json](https://github.com/nlohmann/json)
- [inifile-cpp](https://github.com/Rookfighter/inifile-cpp)
- [utfcpp](https://github.com/nemtrif/utfcpp)
