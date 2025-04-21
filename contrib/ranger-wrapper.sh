#!/usr/bin/env sh
# This wrapper script is invoked by xdg-desktop-portal-termfilechooser.
#
# For more information about input/output arguments read `xdg-desktop-portal-termfilechooser(5)`

set -ex
PATH="${PATH:-'/usr/bin:/bin'}"

multiple="$1"
directory="$2"
save="$3"
path="$4"
out="$5"

cmd="ranger"
termcmd="${TERMCMD:-kitty --title 'termfilechooser'}"

if [ "$save" = "1" ]; then
    # save a file
    set -- --choosefile="$out" --cmd="echo Select save path (see tutorial in preview pane; try pressing zv or zp if no preview)" --selectfile="$path"
elif [ "$directory" = "1" ]; then
    # upload files from a directory
    set -- --choosedir="$out" --show-only-dirs --cmd="echo Select directory (quit in dir to select it)" "$path"
elif [ "$multiple" = "1" ]; then
    # upload multiple files
    set -- --choosefiles="$out" --cmd="echo Select file(s) (open file to select it; <Space> to select multiple)" "$path"
else
    # upload only 1 file
    set -- --choosefile="$out" --cmd="echo Select file (open file to select it)" "$path"
fi

command="$termcmd $cmd"
for arg in "$@"; do
    # escape double quotes
    escaped=$(printf "%s" "$arg" | sed 's/"/\\"/g')
    # escape spaces
    command="$command \"$escaped\""
done

sh -c "$command"
