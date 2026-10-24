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

cmd="yazi"

run_command() {
    if [ "$save" = "1" ]; then
        # save a file
        set -- "$@" $cmd --chooser-file="$out" "$path"
    elif [ "$directory" = "1" ]; then
        # upload files from a directory
        set -- "$@" $cmd --chooser-file="$out" --cwd-file="$out"".1" "$path"
    elif [ "$multiple" = "1" ]; then
        # upload multiple files
        set -- "$@" $cmd --chooser-file="$out" "$path"
    else
        # upload only 1 file
        set -- "$@" $cmd --chooser-file="$out" "$path"
    fi

    "$@" # this runs the whole command: $TERMCMD $cmd $@
}
run_command "$@" # $@ == TERMCMD

if [ "$directory" = "1" ]; then
    if [ ! -s "$out" ] && [ -s "$out"".1" ]; then
        cat "$out"".1" > "$out"
        rm "$out"".1"
    else
        rm "$out"".1"
    fi
fi
