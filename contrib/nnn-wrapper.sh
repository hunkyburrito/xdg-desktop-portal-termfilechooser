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

if [ "$directory" = "1" ] && [ -s "$out" ]; then
    # select on quit; file data will be `cd '/dir/path'`
    if [ "$(cut -c -2 "$out")" = "cd" ]; then
        sed -i "s/^cd '\(.*\)'/\1/" "$out"
    fi
fi
