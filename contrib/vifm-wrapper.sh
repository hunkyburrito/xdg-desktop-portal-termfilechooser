#!/usr/bin/env bash

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
cmd="vifm"
# "wezterm start --always-new-process" if you use wezterm
termcmd="${TERMCMD:-kitty}"
# change this to "/tmp/xxxxxxx/.last_selected" if you only want to save last selected location
# in session (flushed after reset device)
last_selected_path_cfg="$HOME/.config/xdg-desktop-portal-termfilechooser/.last_selected"
mkdir -p "$(dirname last_selected_path_cfg)"
if [ ! -f "$last_selected_path_cfg" ]; then
    touch "$last_selected_path_cfg"
fi
last_selected="$(cat "$last_selected_path_cfg")"

Restore last selected path
if [ -d "$last_selected" ]; then
    # Save/download file
    save_to_file=""
    if [ "$save" = "1" ]; then
        save_to_file="$(basename "$path")"
        path="${last_selected}/${save_to_file}"
    else
        path="${last_selected}"
    fi
fi

if [[ -z "$path" ]]; then
    path="$HOME"
fi

if [ "$save" = "1" ]; then
    # Save/download file
    set -- --choose-files "$out" -c "view" -c "map <esc> :cquit<cr>" -c "set statusline='Select save path (see tutorial in preview pane; try pressing <w> key if no preview)'" --select "$path"
	printf '%b' "
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!            === \033[1mWARNING!\033[0m ===              !!!
!!! The contents of *\033[1mwhatever\033[0m* file you open !!!
!!! last in vifm, will be ***\033[1moverwritten\033[0m***! !!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
>>>>>>> 5ad2dfc (improve vifm wrapper script)

\033[1mNotice:\033[0m
1) The file under \033[1mcursor\033[0m is provided for your
   convenience. You can delete it and choose
   another file to overwrite that file.
2) If you \033[1m:quit\033[0m vifm, this file will be removed
   and the save operation will be aborted.
3) However, if you abort save operation after
   renaming the file, it won't be removed.

\033[1mInstructions\033[0m:
1) Move this file wherever you want.
2) Rename this file as necessary.
3) Confirm selection by pressing <Enter>.
4) To cancel the save operation press <Esc>.
" >"$path"
elif [ "$directory" = "1" ]; then
    # upload files from a directory
	set -- --choose-dir "$out" -c "only" -c "map <esc> :cquit<cr>" -c "set statusline='Select directory (quit in dir to select it, press <Esc> to cancel)'"
elif [ "$multiple" = "1" ]; then
    # upload multiple files
	set -- --choose-files "$out" -c "only" -c "map <esc> :cquit<cr>" -c "set statusline='Select file(s) (press <t> key to select multiple, press <Esc> to cancel)'"
else
    # upload only 1 file
	set -- --choose-files "$out" -c "only" -c "map <esc> :cquit<cr>" -c "set statusline='Select file (open file to select it, press <Esc> to cancel)'"
fi
$termcmd -- $cmd "$@"

# Remove file if the save operation aborted
if [ "$save" = "1" ] && [ ! -s "$out" ]; then
    rm "$path"
else
    # Save the last selected path for the next time, only download file operation is need to use this path, \
    # the other three save last visited location automatically
    selected_path=$(head -n 1 <"$out")
    if [[ -d "$selected_path" ]]; then
        echo "$selected_path" >"$last_selected_path_cfg"
    elif [[ -f "$selected_path" ]]; then
        dirname "$selected_path" >"$last_selected_path_cfg"
    fi
fi
