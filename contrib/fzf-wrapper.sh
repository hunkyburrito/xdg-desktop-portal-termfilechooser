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

termcmd="${TERMCMD:-kitty --title 'termfilechooser'}"

if [ "$save" = "1" ]; then
    # save a file
    cmd="dialog --yesno \"Save to \"$path\"?\" 0 0 && ( printf '%s' \"$path\" > $out ) || ( printf '%s' 'Input path to write to: ' && read input && printf '%s' \"\$input\" > $out)"
elif [ "$directory" = "1" ]; then
    # upload files from a directory
    cmd="fd -a --base-directory=$HOME -td | fzf +m --prompt 'Select directory > ' > $out"
elif [ "$multiple" = "1" ]; then
    # upload multiple files
    cmd="fd -a --base-directory=$HOME | fzf -m --prompt 'Select files > ' > $out"
else
    # upload only 1 file
    cmd="fd -a --base-directory=$HOME | fzf +m --prompt 'Select file > ' > $out"
fi

sh -c "$termcmd sh -c \"$cmd\""
