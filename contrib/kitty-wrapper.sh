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

set -e

if [ "$debug" = 1 ]; then
    set -x
fi

case "$multiple$directory$save" in
    000) mode=file ;;
    100) mode=files ;;
    010) mode=dir ;;
    001) mode=save-file ;;
    110) mode=dirs ;;
    101) mode=save-files ;;
    011) mode=save-dir ;;
    111) mode=save-dir ;;
    *) echo "invalid params" >&2; exit 1
esac

touch $out

if [ "$save" = 1 ]; then
    escaped=$(printf "%s" "$path" | sed 's/"/\\"/g')
    kitty --class filechooser kitty +kitten choose-files --mode=$mode \
        --write-output-to "$out" --suggested-save-file-name "\"$escaped\""
else
    kitty --class filechooser kitty +kitten choose-files --mode=$mode \
        --write-output-to "$out"
fi
