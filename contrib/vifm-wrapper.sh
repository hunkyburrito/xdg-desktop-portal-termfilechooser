#!/usr/bin/env sh
# This wrapper script is invoked by xdg-desktop-portal-termfilechooser.
#
# For more information about input/output arguments read `xdg-desktop-portal-termfilechooser(5)`

set -e

if [ "$6" -ge 4 ]; then
    set -x
fi

multiple="$1"
directory="$2"
save="$3"
path="$4"
out="$5"

cmd="vifm"
termcmd="${TERMCMD:-kitty --title 'termfilechooser'}"

if [ "$save" = "1" ]; then
    # save a file
	set -- --choose-files "$out" -c "only" -c "map <esc> :cquit<cr>" \
		-c "set statusline='Save file (press <Enter> to select or <Esc> to cancel)%NCursorfile is the recommended choice, you can rename/move it%NIf you select another file, it will be overwritten by the save'" \
		--select "$path" >"$path"
elif [ "$directory" = "1" ]; then
    # upload files from a directory
	set -- --choose-files "$out" --choose-dir "$out" -c "only" -c "map <esc> :cquit<cr>" -c "set statusline='Select directory (:quit in dir or select and open it, press <Esc> to cancel)'"
elif [ "$multiple" = "1" ]; then
    # upload multiple files
	set -- --choose-files "$out" -c "only" -c "map <esc> :cquit<cr>" -c "set statusline='Select file(s) (press <t> key to select multiple, press <Esc> to cancel)'"
else
    # upload only 1 file
	set -- --choose-files "$out" -c "only" -c "map <esc> :cquit<cr>" -c "set statusline='Select file (open file to select it, press <Esc> to cancel)'"
fi

command="$termcmd $cmd"
for arg in "$@"; do
    # escape double quotes
    escaped=$(printf "%s" "$arg" | sed 's/"/\\"/g')
    # escape special
    case "$termcmd" in
	*"ghostty"*)
	    command="$command \"\\\"$escaped\\\"\"";;
	*)
	    command="$command \"$escaped\"";;
    esac
done

sh -c "$command"
