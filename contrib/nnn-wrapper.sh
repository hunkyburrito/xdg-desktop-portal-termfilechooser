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

shift 6 # remaining positional args are TERMCMD

set -e

if [ "$debug" = 1 ]; then
    set -x
fi

cmd="nnn"

run_command() {
    if [ "$save" = "1" ]; then
        # save a file
        set -- "$@" $cmd -p "$out" "$path"
    elif [ "$directory" = "1" ]; then
        # upload files from a directory
        set -- env NNN_TMPFILE="$out" "$@" $cmd -p "$out" "$path"
    elif [ "$multiple" = "1" ]; then
        # upload multiple files
        set -- "$@" $cmd -p "$out" "$path"
    else
        # upload only 1 file
        set -- "$@" $cmd -p "$out" "$path"
    fi

    "$@" # this runs the whole command: $TERMCMD $cmd $@
}
run_command "$@" # $@ == TERMCMD

if [ "$directory" = "1" ] && [ -s "$out" ]; then
    # select on quit; file data will be `cd '/dir/path'`
    if [ "$(cut -c -2 "$out")" = "cd" ]; then
        sed -i "s/^cd '\(.*\)'/\1/" "$out"
    fi
fi
