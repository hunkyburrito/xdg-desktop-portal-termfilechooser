#!/usr/bin/env sh
# This wrapper script is invoked by xdg-desktop-portal-termfilechooser.
#
# For more information about input/output arguments read `xdg-desktop-portal-termfilechooser(5)`

multiple="$1"
directory="$2"
save="$3"
path="$4"
out="$5"
debug="$6"

set -e

if [ "$debug" = 1 ]; then
    set -x
fi

cmd="nnn"
termcmd="${TERMCMD:-kitty --title 'termfilechooser'}"

if [ "$save" = "1" ]; then
    # save a file
    set -- -p "$out" "$path"
elif [ "$directory" = "1" ]; then
    # upload files from a directory
    set -- -p "$out" "$path"
elif [ "$multiple" = "1" ]; then
    # upload multiple files
    set -- -p "$out" "$path"
else
    # upload only 1 file
    set -- -p "$out" "$path"
fi

if [ "$directory" = "1" ]; then
    eval env NNN_TMPFILE="$out" $termcmd $cmd "$@"
else
    eval $termcmd $cmd "$@"
fi

if [ "$directory" = "1" ] && [ -s "$out" ]; then
    # select on quit; file data will be `cd '/dir/path'`
    if [ "$(cut -c -2 "$out")" = "cd" ]; then
        sed -i "s/^cd '\(.*\)'/\1/; s/'\\\''/'/g" "$out"
    fi
fi
