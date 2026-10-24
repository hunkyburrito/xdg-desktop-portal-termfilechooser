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

cmd="ranger"

run_command() {
    if [ "$save" = "1" ]; then
        # save a file
        set -- "$@" $cmd --choosefile="$out" --cmd="echo Select save path (see tutorial in preview pane; try pressing zv or zp if no preview)" --selectfile="$path"
    elif [ "$directory" = "1" ]; then
        # upload files from a directory
        set -- "$@" $cmd --choosedir="$out" --show-only-dirs --cmd="echo Select directory (quit in dir to select it)" "$path"
    elif [ "$multiple" = "1" ]; then
        # upload multiple files
        set -- "$@" $cmd --choosefiles="$out" --cmd="echo Select file(s) (open file to select it; <Space> to select multiple)" "$path"
    else
        # upload only 1 file
        set -- "$@" $cmd --choosefile="$out" --cmd="echo Select file (open file to select it)" "$path"
    fi

    "$@" # this runs the whole command: $TERMCMD $cmd $@
}
run_command "$@" # $@ == TERMCMD
