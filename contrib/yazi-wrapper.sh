#!/bin/sh

set -x

# This wrapper script is invoked by xdg-desktop-portal-termfilechooser.
#
# Inputs:
# 1. "1" if multiple files can be chosen, "0" otherwise.
# 2. "1" if a directory should be chosen, "0" otherwise.
# 3. "0" if opening files was requested, "1" if writing to a file was
#    requested. For example, when uploading files in Firefox, this will be "0".
#    When saving a web page in Firefox, this will be "1".
# 4. If writing to a file, this is recommended path provided by the caller. For
#    example, when saving a web page in Firefox, this will be the recommended
#    path Firefox provided, such as "~/Downloads/webpage_title.html".
#    Note that if the path already exists, we keep appending "_" to it until we
#    get a path that does not exist.
# 5. The output path, to which results should be written.
#
# Output:
# The script should print the selected paths to the output path (argument #5),
# one path per line.
# If nothing is printed, then the operation is assumed to have been canceled.

multiple="$1"
directory="$2"
save="$3"
path="$4"
out="$5"
cmd="yazi"
# "wezterm start --always-new-process" if you use wezterm
termcmd="${TERMCMD:-kitty --title 'termfilechooser'}"
# change this to "/tmp/xxxxxxx/.last_selected" if you only want to save last selected location
# in session (flushed after reset device)
last_selected_path_cfg="${XDG_STATE_HOME:-$HOME/.local/state}/xdg-desktop-portal-termfilechooser/last_selected"
mkdir -p "$(dirname "$last_selected_path_cfg")"
if [ ! -f "$last_selected_path_cfg" ]; then
    touch "$last_selected_path_cfg"
fi
last_selected="$(cat "$last_selected_path_cfg")"

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
    # Save/download file
    printf '%s' 'xdg-desktop-portal-termfilechooser saving files tutorial

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!                 === WARNING! ===                 !!!
!!! The contents of *whatever* file you open last in !!!
!!! yazi will be *overwritten*!                    !!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

Instructions:
1) Move this file wherever you want.
2) Rename the file if needed.
3) Confirm your selection by opening the file, for
   example by pressing <Enter>.

Notes:
1) This file is provided for your convenience. You
   could delete it and choose another file to overwrite
   that.
2) If you quit yazi without opening a file, this file
   will be removed and the save operation aborted.
' >"$path"
    set -- --chooser-file="$out" --cwd-file="$last_selected_path_cfg" "$path"
elif [ "$directory" = "1" ]; then
    # upload files from a directory
    set -- --chooser-file="$out" --cwd-file="$last_selected_path_cfg" "$path"
elif [ "$multiple" = "1" ]; then
    # upload multiple files
    set -- --chooser-file="$out" --cwd-file="$last_selected_path_cfg" "$path"
else
    # upload only 1 file
    set -- --chooser-file="$out" --cwd-file="$last_selected_path_cfg" "$path"
fi

command="$termcmd $cmd"
for arg in "$@"; do
    # escape double quotes
    escaped=$(printf "%s" "$arg" | sed 's/"/\\"/g')
    # escape spaces
    command="$command \"$escaped\""
done
sh -c "$command"

# Save the last selected path for the next time, only upload files from a directory operation is need
# because `--cwd-file` will do the same thing for files(s) upload and download operations
if [ "$save" = "0" ] && [ "$directory" = "1" ]; then
    echo "$(head -n 1 <"$out")" >"$last_selected_path_cfg"
fi

# Remove file if the save operation aborted
if [ "$save" = "1" ] && [ ! -s "$out" ]; then
    rm "$path"
fi
