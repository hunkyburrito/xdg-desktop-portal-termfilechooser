#!/bin/sh bash

set -x
# This wrapper script is invoked by xdg-desktop-portal-termfilechooser.
#
# See `ranger-wrapper.sh` for the description of the input arguments and the output format.

multiple="$1"
directory="$2"
save="$3"
path="$4"
out="$5"
# termcmd="x-terminal-emulator -e"
termcmd="${TERMCMD:-kitty --title 'termfilechooser'}"
cmd="nnn -S -s xdg-portal-filechooser"
# -S [-s <session_file_name>] saves the last visited dir location and opens it on next startup

# See also: https://github.com/jarun/nnn/wiki/Basic-use-cases#file-picker

# nnn has no equivalent of ranger's:
# `--cmd`
# .. and no other way to show a message text on startup. So, no way to show instructions in nnn itself, like it is done in ranger-wrapper.
# nnn also does not show previews (needs a plugin and a keypress). So, the save instructions in `$path` file are not shown automatically.
# `--show-only-dirs`
# `--choosedir`
# Navigating to a dir and quitting nnn would not write it to the selection file. To select a dir, use <Space> on a dir name, then quit nnn.
# Although perhaps it could be scripted together with https://github.com/jarun/nnn/wiki/Basic-use-cases#configure-cd-on-quit
# This missing functionality probably could be implemented with a plugin.

if [ -z "$path" ]; then
    path="$HOME"
fi

create_save_file() {
    cat >"$path" <<-'END'
	xdg-desktop-portal-termfilechooser saving files tutorial

	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	!!!                 === WARNING! ===                 !!!
	!!! The contents of *whatever* file you open last in !!!
	!!! nnn will be *overwritten*!                       !!!
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	Instructions:
	1) Move this file wherever you want.
	2) Rename the file if needed.
	3) Confirm your selection by opening the file, for
		 example by pressing <Enter>.

	Notes:
	1) This file is provided for your convenience. You
		 could delete it and choose another file to overwrite
		 that, for example.
	2) If you quit nnn without opening a file, this file
     will be removed and the save operation aborted.
	END
    rm "$out" || : # Remove the old /tmp/termfilechooser.portal
    cleanup() {
        # Remove the file with the above tutorial text if the calling program did not overwrite it.
        if [ ! -s "$out" ] || [ "$path" != "$(cat "$out")" ]; then
            rm "$path" || :
        fi
    }
    trap cleanup EXIT HUP INT QUIT ABRT TERM
}

if [ "$save" = "1" ]; then
    create_save_file
    set -- -p "$out" "$path"
else
    set -- -p "$out"
fi

# data will be `cd "/dir/path"`
NNN_TMPFILE_PATH="/tmp/xdg-desktop-portal-termfilechooser-NNN-tmpfile"
if [ "$directory" -eq "1" ]; then
    env NNN_TMPFILE="$NNN_TMPFILE_PATH" "$termcmd" -- "$cmd" "$@"
else
    sh -c "$termcmd -- $cmd $*"
fi

if [ "$directory" -eq "1" ] && [ -n "$NNN_TMPFILE_PATH" ]; then
    LAST_SELECTED_DIR=$(cat "$NNN_TMPFILE_PATH")
    # convert data from `cd "/dir/path"`  to "/dir/path"
    LAST_SELECTED_DIR=$(echo "$LAST_SELECTED_DIR" | sed -n "s/^cd '\(.*\)'/\1/p")
    echo "$LAST_SELECTED_DIR" >"$out"
fi
