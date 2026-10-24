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

cmd="vifm"

run_command() {
    if [ "$save" = "1" ]; then
        # save a file
        set -- "$@" $cmd --choose-files "$out" -c "only" -c "map <esc> :cquit<cr>" \
        -c "set statusline='Save file (press <Enter> to select or <Esc> to cancel)%NCursorfile is the recommended choice, you can rename/move it%NIf you select another file, it will be overwritten by the save'" \
        --select "$path" >"$path"
    elif [ "$directory" = "1" ]; then
        # upload files from a directory
        set -- "$@" $cmd --choose-files "$out" --choose-dir "$out" -c "only" -c "map <esc> :cquit<cr>" -c "set statusline='Select directory (:quit in dir or select and open it, press <Esc> to cancel)'"
    elif [ "$multiple" = "1" ]; then
        # upload multiple files
        set -- "$@" $cmd --choose-files "$out" -c "only" -c "map <esc> :cquit<cr>" -c "set statusline='Select file(s) (press <t> key to select multiple, press <Esc> to cancel)'"
    else
        # upload only 1 file
        set -- "$@" $cmd --choose-files "$out" -c "only" -c "map <esc> :cquit<cr>" -c "set statusline='Select file (open file to select it, press <Esc> to cancel)'"
    fi

    "$@" # this runs the whole command: $TERMCMD $cmd $@
}
run_command "$@" # $@ == TERMCMD
