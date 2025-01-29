xdg-desktop-portal-termfilechooser(5)

# NAME

xdg-desktop-portal-termfilechooser - an xdg-desktop-portal backend to choose
files with your favorite terminal file manager.

# DESCRIPTION

xdg-desktop-portal-termfilechooser (or xdptf for short) allows applications to
choose files via xdg-desktop-portal using your favorite terminal filechooser.

xdptf will try to load the configuration file from these locations:

- $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/$XDG_CURRENT_DESKTOP
- $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/config
- /etc/xdg/xdg-desktop-portal-termfilechooser/$XDG_CURRENT_DESKTOP
- /etc/xdg/xdg-desktop-portal-termfilechooser/config

_$XDG_CONFIG_HOME_ defaults to _~/.config_.
_$XDG_CURRENT_DESKTOP_ can be a colon seperated list. Each element of that list
will be tried.

# WRAPPERS

Wrappers are shell scripts that take in a number of arguments and launch the
file manager with the correct settings to select files. Refer to **WRAPPER
ARGUMENTS** for specifics.

There are several included wrappers provided in either
'/usr/local/share/xdg-desktop-portal-termfilechooser or
'/usr/share/xdg-desktop-portal-termfilechooser' by default.

These wrappers are designed to use the environment variable 'TERMCMD' for
launching a terminal emulator. If 'TERMCMD' is not set, kitty is used as
the default terminal emulator.

## WRAPPER ARGUMENTS

These arguments need to be captured by the wrapper to allow the selection of
files.

Name - **multiple**
Description - Selection of multiple files.
Position - Argument 1
Value - boolean < 0 | 1 >

Name - **directory**
Description - Selection of a directory.
Position - Argument 2
Value - boolean < 0 | 1 >

Name - **save**
Description - Selection for saving files.
Position - Argument 3
Value - boolean < 0 | 1 >

Name - **path**
Description - The suggested file or directory received from the application.
Position - Argument 4
Value - string < file/directory path >

Name - **out**
Description - The file to write selected files/directories to.
Position - Argument 5
Value - string < file path >

# FILECHOOSER CONFIGURATION

The configuration file uses the INI file format. The only implemented section is
**[filechooser]** at the time of writing. Keys declared multiple times are
overwritten with the value of the last declaration (except for 'env' described
below).

## FILECHOOSER OPTIONS

These options need to be placed under the **[filechooser]** section.

**cmd** = _command_
	Command to execute. This is typically set to a wrapper script.
	For invocation details, please refer to the default wrapper script.

	If the command is not a full path, a modified PATH environment variable is
	searched for commands/scripts.

	The PATH modification prioritizes in the order of:
	- $XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser
	- /usr/local/share/xdg-desktop-portal-termfilechooser
	- /usr/share/xdg-desktop-portal-termfilechooser
	- Global $PATH

	The default value is yazi-wrapper.sh

**default_dir** = _directory_
	Default directory to save files in if the invoking program does not
	provide one.

	The default value is /tmp.

**env** = _name_=_value_
	Environment variable to launch 'cmd' with. This key allows for several
	environment variables to be set. Either set 'env=' multiple times, or indent
	the values as shown in **EXAMPLE CONFIG**

## EXAMPLE CONFIG

```
[filechooser]
cmd=yazi-wrapper.sh
default_dir=$HOME/Downloads
env=TERMCMD=foot
	EDITOR=nvim
```
