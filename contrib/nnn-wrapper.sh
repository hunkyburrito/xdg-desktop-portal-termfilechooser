#!/bin/sh

set -x
# This wrapper script is invoked by xdg-desktop-portal-termfilechooser.
#
# See `ranger-wrapper.sh` for the description of the input arguments and the output format.

multiple="$1"
directory="$2"
save="$3"
path="$4"
out="$5"
termcmd="${TERMCMD:-kitty --title 'termfilechooser'}"
cmd="nnn -S -s termfilechooser"
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

last_selected_path="${XDG_STATE_HOME:-$HOME/.local/state}/xdg-desktop-portal-termfilechooser/last_selected"
mkdir -p "$(dirname "$last_selected_path")"
if [ ! -f "$last_selected_path" ]; then
    touch "$last_selected_path"
fi
last_selected="$(cat "$last_selected_path")"

# Restore last selected path
if [ -d "$last_selected" ]; then
    save_to_file=""
    if [ "$save" = "1" ]; then
        save_to_file="$(basename "$path")"
        path="${last_selected}/${save_to_file}"
    else
        path="${last_selected}"
    fi
fi

if [ -z "$path" ]; then
    path="$HOME"
fi

if [ "$save" = "1" ]; then
    # Save tutorial
    printf "%s" 'xdg-desktop-portal-termfilechooser saving files tutorial

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
' >"$path"
    set -- -p "$out" "$path"
else
    set -- -p "$out" "$path"
fi

command="$termcmd $cmd"
for arg in "$@"; do
    # escape double quotes
    escaped=$(printf "%s" "$arg" | sed 's/"/\\"/g')
    # escape spaces
    command="$command \"$escaped\""
done

if [ "$directory" = "1" ]; then
    sh -c "env NNN_TMPFILE=\"$out\" $command"
else
    sh -c "$command"
fi

NNN_QUIT=0
if [ "$save" = "0" ] && [ "$directory" = "1" ] && [ -s "$out" ]; then
    # select on quit; file data will be `cd '/dir/path'`
    if [ "$(cut -c -2 "$out")" = "cd" ]; then
        sed -i "s/^cd '\(.*\)'/\1/" "$out"
        NNN_QUIT=1
    fi
fi

# Remove the file with the above tutorial text if the calling program did not overwrite it.
if [ "$save" = "1" ] && [ ! -s "$out" ] || [ "$path" != "$(cat "$out")" ]; then
    rm "$path"
else
    selected_path="$(tail -n 1 "$out")"
    if [ "$NNN_QUIT" = "1" ]; then
        echo "$selected_path" >"$last_selected_path"
    else
        dirname "$selected_path" >"$last_selected_path"
    fi
fi
