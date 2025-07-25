xdg-desktop-portal-termfilechooser(5)

# NAME

xdg-desktop-portal-termfilechooser - an xdg-desktop-portal backend to choose
files with your favorite terminal file manager.

# DESCRIPTION

xdg-desktop-portal-termfilechooser (or xdptf for short) allows applications to
choose files via xdg-desktop-portal using your favorite terminal filechooser.

xdptf will try to load the configuration file from these locations:

- _$XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/$XDG_CURRENT_DESKTOP_
- _$XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/config_
- _/etc/xdg/xdg-desktop-portal-termfilechooser/$XDG_CURRENT_DESKTOP_
- _/etc/xdg/xdg-desktop-portal-termfilechooser/config_

_$XDG_CONFIG_HOME_ defaults to _~/.config_.
_$XDG_CURRENT_DESKTOP_ can be a colon seperated list. Each element of that list
will be tried.

# WRAPPERS

Wrappers are shell scripts that take in a number of arguments and launch the
file manager with the correct settings to select files. Refer to *WRAPPER
ARGUMENTS* for specifics.

There are several included wrappers provided in either
_/usr/local/share/xdg-desktop-portal-termfilechooser_ or
_/usr/share/xdg-desktop-portal-termfilechooser_ by default.

These wrappers are designed to use the environment variable *TERMCMD* for
launching a terminal emulator. If *TERMCMD* is not set, kitty is used as
the default terminal emulator.

## WRAPPER ARGUMENTS

These arguments need to be captured by the wrapper to allow the selection of
files.

*Name*: _multiple_ ++
*Description*: Selection of multiple files.++
*Position*: Argument 1++
*Value*: boolean < 0 | 1 >

*Name*: _directory_ ++
*Description*: Selection of a directory.++
*Position*: Argument 2++
*Value*: boolean < 0 | 1 >

*Name*: _save_ ++
*Description*: Selection for saving files.++
*Position*: Argument 3++
*Value*: boolean < 0 | 1 >

*Name*: _path_ ++
*Description*: The suggested file or directory received from the application.++
*Position*: Argument 4++
*Value*: string < file/directory path >

*Name*: _out_ ++
*Description*: The file to write selected files/directories to.++
*Position*: Argument 5++
*Value*: string < file path >

*Name*: _N/A_ ++
*Description*: The loglevel set for termfilechooser.++
*Position*: Argument 6++
*Vaule*: int

# FILECHOOSER CONFIGURATION

The configuration file uses the INI file format. The only implemented section is
*[filechooser]* at the time of writing. Keys declared multiple times are
overwritten with the value of the last declaration (except for *env* described
below).

## FILECHOOSER OPTIONS

These options need to be placed under the *[filechooser]* section.

*cmd* = _command_
	Command to execute. This is typically set to a wrapper script.
	For invocation details, please refer to the default wrapper script.

	If the command is not a full path, a modified PATH environment variable is
	searched for commands/scripts.

	The modified PATH prioritizes searching as follows:
	- _$XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser_
	- _/usr/local/share/xdg-desktop-portal-termfilechooser_
	- _/usr/share/xdg-desktop-portal-termfilechooser_
	- Global *$PATH*

	The default value is yazi-wrapper.sh

*create_help_file* = _bool_
	Populates the destination save file with instructions.

	Accepted values are *0* and *1*.

	The default value is *1*.

*default_dir* = _directory_
	Default directory to open in your file manager if the invoking program does not
	provide one.

	The default value is *$HOME* with a fallback of */tmp*.

*env* = _name_=_value_
	Environment variable to launch *cmd* with. This key allows for several
	environment variables to be set. Either set *env=* multiple times, or indent
	the values as shown in *EXAMPLE CONFIG*

*open_mode* = _mode_
	Sets what path the file manager starts in when selecting
	files/directories. The _mode_ needs to be one of *suggested*, *default*, or
	*last*. A brief description of these modes is:

	_suggested_ - Uses the application's suggested path. ++
_default_ - Uses the value set for *default_dir*.++
_last_ - Uses the parent directory of the last selected file. If the last
	selection was a directory, that is used instead.

	The default value is *suggested*.

*save_mode* = _mode_
	Sets what path the file manager starts in when saving files. The _mode_ needs to be one of *suggested*, *default*, or
	*last*.

	_suggested_ - Uses the application's suggested path. ++
_default_ - Uses the value set for *default_dir*.++
_last_ - Uses the parent directory of the last selected file. If the last
	selection was a directory, that is used instead.

	The default value is *suggested*.

## EXAMPLE CONFIG

```
[filechooser]
cmd=yazi-wrapper.sh
create_help_file=0
default_dir=$HOME
env=TERMCMD=foot
	EDITOR=nvim
open_mode=suggested
save_mode=last
```
