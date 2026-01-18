# Compatability
## Generic Workarounds
- For GTK applications:
    - Try setting the `GDK_DEBUG=portals` environment variable when running the application. If that doesn't help, try using the [deprecated](https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/4829) `GTK_USE_PORTAL=1` instead.
- For Electron applications:
    - Try using a package built against a newer electron version. Older electron versions do not support the filechooser portal, and some versions have broken support (see https://github.com/electron/electron/issues/43819).
- For Qt applications:
    - Try setting `QT_QPA_PLATFORMTHEME=xdgdesktopportal` when running the application. Alternatively, set the dialogs to `XDG Desktop Portal` within `qt5ct/qt6ct` (or other Qt style configuration programs) to preserve your other theme settings.

## Special Configuration
- Firefox: Set `widget.use-xdg-desktop-portal.file-picker` to `1` in `about:config`. See the [ArchWiki](https://wiki.archlinux.org/title/Firefox#XDG_Desktop_Portal_integration) for more information.

## Broken
- Gimp (01/18/2026)
- KeepassXC (01/18/2026)
- PCSX2 (01/18/2026)
