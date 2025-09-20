# xdg-desktop-portal-termfilechooser

[xdg-desktop-portal] backend for choosing files with your favorite terminal file manager.

<!--toc:start-->

## Table of Contents

- [Features](#features)
- [Installation](#installation)
    - [Packages](#packages)
    - [Building from source](#building-from-source)
        - [Download source](#download-source)
        - [Dependencies](#dependencies)
        - [Build](#build)
- [Configuration](#configuration)
- [Usage](#usage)
    - [Launching termfilechooser](#launching-termfilechooser)
    - [Selection](#selection)
    - [Tips](#tips)
- [Troubleshooting](#troubleshooting)
- [Documentation](#documentation)
- [License](#license)
- [Credits](#credits)

<!--toc:end-->

## Features

- Single file selection
- Multi-file selection
- Directory selection
- Multi-user support
- Customizable execution through shell scripts

## Installation

### Packages

[Arch Linux (AUR)](https://aur.archlinux.org/packages/xdg-desktop-portal-termfilechooser-hunkyburrito-git)

    yay -S xdg-desktop-portal-termfilechooser-hunkyburrito-git

[NixOS](https://github.com/NixOS/nixpkgs/tree/master/pkgs/by-name/xd/xdg-desktop-portal-termfilechooser)

### Building from source

#### Download source

    git clone https://github.com/hunkyburrito/xdg-desktop-portal-termfilechooser

#### Dependencies

Install the required packages for your distribution.

On apt-based systems:

    sudo apt install xdg-desktop-portal build-essential ninja-build meson libinih-dev libsystemd-dev scdoc

On Alpine/postmarketOS:

    sudo apk add meson musl-dev ninja-build clang cmake pkgconfig inih-dev basu-dev scdoc xdg-desktop-portal

On Arch-based systems:

    sudo pacman -S xdg-desktop-portal libinih ninja meson scdoc git

#### Build

    cd xdg-desktop-portal-termfilechooser
    meson build
    ninja -C build
    ninja -C build install  # run with superuser privileges

Note: Some distributions require `termfilechooser.portal` to be in `/usr/share/xdg-desktop-portal/portals/`.

Known distributions that require this are Alpine/postmarketOS, Debian, and Arch Linux.

    sudo mv /usr/local/share/xdg-desktop-portal/portals/termfilechooser.portal /usr/share/xdg-desktop-portal/portals/

## Configuration

By default, the contents of the `contrib` folder are placed in `/usr/local/share/xdg-desktop-portal-termfilechooser/`.
Copy the `config` to `$XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/` and edit it to set your preferred wrapper and default directory.

The configuration is as follows:

`[filechooser]`

- `cmd`: The wrapper script/command to run.
- `create_help_file`: Create destination save file with instructions. Must be *0* or *1* (default). See `man 5 xdg-desktop-portal-termfilechooser` for more info.
- `default_dir`: The default directory to open if the application (e.g. firefox) does not suggest a path.
- `env`: Sets the specified environment variables with the specified values.
    - `TERMCMD`: The environment variable that sets what command to use for launching a terminal.
- `open_mode`: Sets the mode for the starting path when selecting files/directories. Must be one of *suggested*, *default*, or *last*. See `man 5 xdg-desktop-portal-termfilechooser` for more info.
- `save_mode`: Sets the mode for the starting path when saving files. Must be one of *suggested*, *default*, or *last*. See `man 5 xdg-desktop-portal-termfilechooser` for more info.

Wrappers specified within the `cmd` key in the `config` are searched for in order of the following directories unless the absolute path is specified.

- `$XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser`
- `/usr/local/share/xdg-desktop-portal-termfilechooser`
- `/usr/share/xdg-desktop-portal-termfilechooser`
- Any other directory in your `$PATH`

Setting `TERMCMD` can be done with the following methods (in recommended order):

- Using the `env` key in the `config`
- Editing the `cmd` key in the `config`
- Exporting a global `TERMCMD` environment variable
- Creating/Editing your own wrapper file

### Examples

#### Using `env`:

Note: Setting environment variables in the config with `env` does not require you to quote the entire value. See example below.

```conf
### $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/config ###

[filechooser]
cmd=yazi-wrapper.sh
default_dir=$HOME
env=TERMCMD=foot -T "terminal filechooser"
    VARIABLE2=VALUE2
open_mode = suggested
save_mode = last
```

OR

```conf
### $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/config ###

[filechooser]
cmd=yazi-wrapper.sh
default_dir=$HOME
env=TERMCMD=foot -T "terminal filechooser"
env=VARIABLE2=VALUE2
open_mode = suggested
save_mode = last
```

Environment variables can also be unset. (e.g. `env=VARIABLE=`)

#### Editing `cmd`:

Note: Setting environment variables through prepending requires proper quoting and necessary escaping. See example below.

```conf
### $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/config ###

[filechooser]
cmd=TERMCMD='foot -T "terminal filechooser"' yazi-wrapper.sh
default_dir=$HOME
open_mode = suggested
save_mode = last
```

#### Exporting a global:

Note: Setting `TERMCMD` globally requires proper quoting and necessary escaping. See example below.

```sh
### $HOME/.profile, .bashrc, or equivalent ###

export TERMCMD='foot -T "terminal filechooser"'
```

#### Modifying a wrapper:

    cp /usr/local/share/xdg-desktop-portal-termfilechooser/yazi-wrapper.sh $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/yazi-wrapper.sh

Make your changes, then:

```conf
### $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/config ###

[filechooser]
# prioritizes `yazi-wrapper.sh` in `$XDG_CONFIG_HOME` dir over `/usr/local/share` and `/usr/share` dirs
cmd=yazi-wrapper.sh
default_dir=$HOME
open_mode = suggested
save_mode = last
```

## Usage

### Launching termfilechooser

#### Systemd service

The simplest way to use this program is through the provided `systemd` service.
Once installed, it should automatically start running after restarting the [xdg-desktop-portal] service.

```
systemctl --user restart xdg-desktop-portal.service
```

#### Manual execution

If you encounter issues or prefer launching manually, you can start termfilechooser yourself.

Note: Depending on the installation method the binary could have a different path.

    /usr/local/lib/xdg-desktop-portal-termfilechooser -r &

### Selection

File(s) selection is pretty straightforward - select and then "open" the files.

Directory selection can vary depending on the file manager, however. Most file managers support selecting directories by entering them and then quitting, but some also support selecting/opening them like file selection.

| File Manager | "Quit" Selection | "Open" Selection |
| ------------ | ---------------- | ---------------- |
| [lf]         | yes              | no               |
| [nnn]        | yes              | yes              |
| [ranger]     | yes              | no               |
| [vifm]       | yes              | yes              |
| [yazi]       | yes              | yes              |

Note: File managers that use "Quit" Selection often have a keybind or command to abort the selection process. For example, in [yazi] it is 'Q'.

### Tips

#### Firefox

Firefox has a setting in its `about:config` to always use XDG desktop portal's file chooser.

Set `widget.use-xdg-desktop-portal.file-picker` to `1`. See the [ArchWiki](https://wiki.archlinux.org/title/Firefox#XDG_Desktop_Portal_integration) for more information.

#### Disable the original file picker portal

If your xdg-desktop-portal version (`/usr/lib/xdg-desktop-portal --version`) is newer than [`1.18.0`](https://github.com/flatpak/xdg-desktop-portal/releases/tag/1.18.0), you can specify the preferred FileChooser in `$XDG_CONFIG_HOME/xdg-desktop-portal/portals.conf`.

```
    ### $XDG_CONFIG_HOME/xdg-desktop-portal/portals.conf ###

    [preferred]
    org.freedesktop.impl.portal.FileChooser=termfilechooser
```

See the [flatpak docs](https://flatpak.github.io/xdg-desktop-portal/docs/portals.conf.html) and [ArchWiki](https://wiki.archlinux.org/title/XDG_Desktop_Portal#Configuration) for more information on `portals.conf`.

If your version is older, you can remove `FileChooser` from `Interfaces` of the `{gtk;kde;…}.portal` files:

    find /usr/share/xdg-desktop-portal/portals -name '*.portal' -not -name 'termfilechooser.portal' \
    	-exec grep -q 'FileChooser' '{}' \; \
    	-exec sudo sed -i'.bak' 's/org\.freedesktop\.impl\.portal\.FileChooser;\?//g' '{}' \;

## Troubleshooting

### Common issues

1. Wrapper failure (`[ERROR] - filechooser: could not execute ...`)

- Check that the `config` is correct, ensuring there is proper escaping if necessary.
- Try manually running termfilechooser instead (the `systemd` service may have issues with getting/setting the environment).
- Try updating termfilechooser and restarting the relevant `systemd` services.

2. Only some applications work

- For GTK applications, try setting the `GDK_DEBUG=portals` environment variable when running the problematic application. If that doesn't help, try using the [deprecated](https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/4829) `GTK_USE_PORTAL=1` instead.
- For electron applications, try using a package built against a newer electron version. Older electron versions do not support the filechooser portal, and some versions have broken support (see https://github.com/electron/electron/issues/43819).
- For Qt applications, try setting `QT_QPA_PLATFORMTHEME=flatpak` or `QT_QPA_PLATFORMTHEME=xdgdesktopportal` when running the application. Alternatively, you can just set the dialogs to `XDG Desktop Portal` within the `qt5ct/qt6ct` configuration programs to preserve your theme settings.

3. Outdated version

- Ensure you are on the most up-to-date version of `termfilechooser`. The version can be checked by running `/usr/local/lib/xdg-desktop-portal-termfilechooser -v` (correct path as needed).

4. Terminal File Manager does not work with "Open/Show Folder" dialogs

- This is expected behavior. Currently termfilechooser only works for selecting files and folders for downloading/uploading. Dialogs, like the one mentioned, use a different interface - https://www.freedesktop.org/wiki/Specifications/file-manager-interface.
- To get this functionality you can use https://github.com/boydaihungst/org.freedesktop.FileManager1.common

See also: [Troubleshooting section in ArchWiki](https://wiki.archlinux.org/title/XDG_Desktop_Portal#Troubleshooting).

If the above do not help, please continue troubleshooting and submit an issue/PR about the problem.

### Logs

When termfilechooser runs as a `systemd` service, it's output can be viewed with `journalctl`.

    journalctl --user -eu xdg-desktop-portal-termfilechooser

The available logging levels in order of increasing verbosity are: `QUIET`, `ERROR`, `WARN`(default), `INFO`, `DEBUG`, and `TRACE`.
Running the program manually allows you to change the logging level much easier.

Note: Correct the binary path as necessary.

    /usr/local/lib/xdg-desktop-portal-termfilechooser -r -l DEBUG

After running this command, try the problematic actions again and you should see more information on what is going wrong.

### Testing

Using `zenity` can make it easier to quickly test the portal. Remember to restart termfilechooser if you edit the `config`.

    systemctl --user restart xdg-desktop-portal-termfilechooser.service

File selection test:

    GDK_DEBUG=portals zenity --file-selection

Multiple file selection test:

    GDK_DEBUG=portals zenity --file-selection --multiple

Directory selection test:

    GDK_DEBUG=portals zenity --file-selection --directory

Save test:

    GDK_DEBUG=portals zenity --file-selection --save --filename=test.txt

## Documentation

A man page documenting wrapper script arguments and configuration options is provided.
See `man 5 xdg-desktop-portal-termfilechooser`.

## License

MIT

## Credits

- [xdg-desktop-portal](https://github.com/flatpak/xdg-desktop-portal)
- [xdg-desktop-portal-wlr](https://github.com/emersion/xdg-desktop-portal-wlr)
- [fzf](https://github.com/junegunn/fzf)
- [lf](https://github.com/gokcehan/lf)
- [nnn](https://github.com/jarun/nnn)
- [ranger](https://github.com/ranger/ranger)
- [vifm](https://github.com/vifm/vifm)
- [yazi](https://github.com/sxyazi/yazi)

[Original Author: GermainZ](https://github.com/GermainZ/xdg-desktop-portal-termfilechooser)

[Upstream Author: boydaihungst](https://github.com/boydaihungst/xdg-desktop-portal-termfilechooser)

[xdg-desktop-portal]: https://github.com/flatpak/xdg-desktop-portal
[xdg-desktop-portal-wlr]: https://github.com/emersion/xdg-desktop-portal-wlr
[fzf]: https://github.com/junegunn/fzf
[lf]: https://github.com/gokcehan/lf
[nnn]: https://github.com/jarun/nnn
[ranger]: https://github.com/ranger/ranger
[vifm]: https://github.com/vifm/vifm
[yazi]: https://github.com/sxyazi/yazi
