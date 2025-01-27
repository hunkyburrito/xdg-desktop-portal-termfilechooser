# xdg-desktop-portal-termfilechooser

<!--toc:start-->

-   [xdg-desktop-portal-termfilechooser](#xdg-desktop-portal-termfilechooser)
    -   [Installation](#installation)
        -   [Dependencies](#dependencies)
        -   [Download the source](#download-the-source)
        -   [Build](#build)
        -   [Config files](#config-files)
        -   [Disable the original file picker portal](#disable-the-original-file-picker-portal)
        -   [Systemd service](#systemd-service)
        -   [Test](#test)
            -   [Troubleshooting](#troubleshooting)
    -   [Usage](#usage)
    -   [Documentation](#documentation)
    -   [License](#license)
    <!--toc:end-->

[xdg-desktop-portal] backend for choosing files with your favorite file chooser.
By default, it will use the [yazi](https://github.com/sxyazi/yazi) file manager, but this is customizable.
Based on [xdg-desktop-portal-wlr] (xdpw).

## Installation

### Dependencies

Install the required packages for your distribution.

On apt-based systems:

    sudo apt install xdg-desktop-portal build-essential ninja-build meson libinih-dev libsystemd-dev scdoc

On Alpine/postmarketOS:

    sudo apk add meson ninja-build clang cmake pkgconfig inih-dev basu-dev scdoc xdg-desktop-portal

On Arch, either use the available [AUR package](https://aur.archlinux.org/packages/xdg-desktop-portal-termfilechooser-hunkyburrito-git), or use the following:

    sudo pacman -S xdg-desktop-portal libinih ninja meson scdoc git

### Download the source

    git clone https://github.com/hunkyburrito/xdg-desktop-portal-termfilechooser

### Build

    cd xdg-desktop-portal-termfilechooser
    meson build
    ninja -C build
    ninja -C build install  # run with superuser privileges

On Alpine/postmarketOS, copy the configs to the right place:

    mkdir -p ~/.config/xdg-desktop-portal-termfilechooser/
    sudo cp ~/Downloads/xdg-desktop-portal-termfilechooser/contrib/config ~/.config/xdg-desktop-portal-termfilechooser/
    sudo cp ~/Downloads/xdg-desktop-portal-termfilechooser/contrib/yazi-wrapper.sh ~/.config/xdg-desktop-portal-termfilechooser/
    sudo cp ~/Downloads/xdg-desktop-portal-termfilechooser/termfilechooser.portal /usr/share/xdg-desktop-portal/portals/

On Debian, move the `termfilechooser.portal` file:

    sudo mv /usr/local/share/xdg-desktop-portal/portals/termfilechooser.portal /usr/share/xdg-desktop-portal/portals/

### Config files

By default, the contents of the `contrib` folder are placed in `/usr/local/share/xdg-desktop-portal-termfilechooser/`.
Copy the `config` to `~/.config/xdg-desktop-portal-termfilechooser` and edit it to set your preferred wrapper and default directory.

The main options for customizing how the filepicker is launched (in recommended order) are:

- Editing the `env` key in the `config`
- Prepending your environment variables in the `cmd` key in the `config`
- Exporting a global `TERMCMD` environment variable
- Creating/Editing your own wrapper file

#### Example:

##### Editing `env`:

```conf
### $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/config ###

[filechooser]
cmd=/usr/local/share/xdg-desktop-portal-termfilechooser/yazi-wrapper.sh
default_dir=$HOME/Downloads
env=VARIABLE1=VALUE1
    VARIABLE2=VALUE2
```
OR
```conf
### $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/config ###

[filechooser]
cmd=/usr/local/share/xdg-desktop-portal-termfilechooser/yazi-wrapper.sh
default_dir=$HOME/Downloads
env=VARIABLE1=VALUE1
env=VARIABLE2=VALUE2
```

Environment variables that unset values are also allowed. (e.g. `env=VARIABLE=`)

##### Prepending variables:

```conf
### $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/config ###

[filechooser]
cmd=TERMCMD='wezterm start --always-new-process' /usr/local/share/xdg-desktop-portal-termfilechooser/yazi-wrapper.sh
default_dir=$HOME/Downloads
```

##### Exporting a global:

```sh
### $HOME/.profile, .bashrc, or equivalent ###

# use wezterm intead of kitty
export TERMCMD="wezterm start --always-new-process"
```

##### Copying wrapper:

```cp /usr/local/share/xdg-desktop-portal-termfilechooser/yazi-wrapper.sh $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/custom-yazi-wrapper.sh```

```conf
### $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/config ###

[filechooser]
cmd=$XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/custom-yazi-wrapper.sh
default_dir=$HOME/Downloads
```

### Disable the original file picker portal

If your xdg-desktop-portal version

    xdg-desktop-portal --version
    # If xdg-desktop-portal not on $PATH, try:
    /usr/libexec/xdg-desktop-portal --version
    # OR, if it says file not found
    /usr/lib64/xdg-desktop-portal --version


is >= [`1.18.0`](https://github.com/flatpak/xdg-desktop-portal/releases/tag/1.18.0), then you can specify the portal for FileChooser in `~/.config/xdg-desktop-portal/portals.conf` file (see the [flatpak docs](https://flatpak.github.io/xdg-desktop-portal/docs/portals.conf.html) and [ArchWiki](https://wiki.archlinux.org/title/XDG_Desktop_Portal#Configuration)):

    org.freedesktop.impl.portal.FileChooser=termfilechooser

If your `xdg-desktop-portal --version` is older, you can remove `FileChooser` from `Interfaces` of the `{gtk;kde;…}.portal` files:

    find /usr/share/xdg-desktop-portal/portals -name '*.portal' -not -name 'termfilechooser.portal' \
    	-exec grep -q 'FileChooser' '{}' \; \
    	-exec sudo sed -i'.bak' 's/org\.freedesktop\.impl\.portal\.FileChooser;\?//g' '{}' \;

### Systemd service

Restart the portal service:

    systemctl --user restart xdg-desktop-portal.service

### Test

    GTK_USE_PORTAL=1  zenity --file-selection

and additional options: `--multiple`, `--directory`, `--save`.

#### Troubleshooting

-   After editing termfilechooser's config, restart its service:

          systemctl --user restart xdg-desktop-portal-termfilechooser.service

-   The termfilechooser's executable can also be launched directly:

          systemctl --user stop xdg-desktop-portal-termfilechooser.service
          /usr/local/libexec/xdg-desktop-portal-termfilechooser -l TRACE -r &

    or, if it says file/folder not found:

          systemctl --user stop xdg-desktop-portal-termfilechooser.service
          /usr/lib64/xdg-desktop-portal-termfilechooser -l TRACE -r &


    This way the output from the wrapper scripts (e.g. `ranger-wrapper.sh`) will be written to the same terminal. This is handy for using e.g. `set -x` in the scripts during debugging.
    When termfilechooser runs as a `systemd` service, its output can be viewed with `journalctl`.

-   Since [this merge request in GNOME](https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/4829), `GTK_USE_PORTAL=1` seems to be replaced with `GDK_DEBUG=portals`.

-   See also: [Troubleshooting section in ArchWiki](https://wiki.archlinux.org/title/XDG_Desktop_Portal#Troubleshooting).

## Usage

Firefox has a setting in its `about:config` to always use XDG desktop portal's file chooser: set `widget.use-xdg-desktop-portal.file-picker` to `1`. See [https://wiki.archlinux.org/title/Firefox#XDG_Desktop_Portal_integration](https://wiki.archlinux.org/title/Firefox#XDG_Desktop_Portal_integration).

## Documentation

See `man 5 xdg-desktop-portal-termfilechooser`.

## License

MIT

[xdg-desktop-portal]: https://github.com/flatpak/xdg-desktop-portal
[xdg-desktop-portal-wlr]: https://github.com/emersion/xdg-desktop-portal-wlr
[ranger]: https://ranger.github.io/
