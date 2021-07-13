# Accepting configure files

## Reading a configure file

You can tell your app to allow configure files with `set_config("--config")`. There are arguments: the first is the option name. If empty, it will clear the config flag. The second item is the default file name. If that is specified, the config will try to read that file. The third item is the help string, with a reasonable default, and the final argument is a boolean (default: false) that indicates that the configuration file is required and an error will be thrown if the file is not found and this is set to true.

### Extra fields
Sometimes configuration files are used for multiple purposes so CLI11 allows options on how to deal with extra fields

```cpp
app.allow_config_extras(true);
```
will allow capture the extras in the extras field of the app. (NOTE:  This also sets the `allow_extras` in the app to true)

```cpp
app.allow_config_extras(false);
```
will generate an error if there are any extra fields

for slightly finer control there is a scoped enumeration of the modes
or
```cpp
app.allow_config_extras(CLI::config_extras_mode::ignore);
```
will completely ignore extra parameters in the config file.   This mode is the default.

```cpp
app.allow_config_extras(CLI::config_extras_mode::capture);
```
will store the unrecognized options in the app extras fields. This option is the closest equivalent to `app.allow_config_extras(true);` with the exception that it does not also set the `allow_extras` flag so using this option without also setting `allow_extras(true)` will generate an error which may or may not be the desired behavior.

```cpp
app.allow_config_extras(CLI::config_extras_mode::error);
```
is equivalent to `app.allow_config_extras(false);`

### Getting the used configuration file name
If it is needed to get the configuration file name used this can be obtained via
`app.get_config_ptr()->as<std::string>()`  or
`app["--config"]->as<std::string>()` assuming `--config` was the configuration option name.

## Configure file format

Here is an example configuration file, in INI format:

```ini
; Comments are supported, using a ;
; The default section is [default], case insensitive

value = 1
str = "A string"
vector = 1 2 3

; Section map to subcommands
[subcommand]
in_subcommand = Wow
sub.subcommand = true
```

Spaces before and after the name and argument are ignored. Multiple arguments are separated by spaces. One set of quotes will be removed, preserving spaces (the same way the command line works). Boolean options can be `true`, `on`, `1`, `y`, `t`, `+`, `yes`, `enable`; or `false`, `off`, `0`, `no`, `n`, `f`, `-`, `disable`, (case insensitive). Sections (and `.` separated names) are treated as subcommands (note: this does not necessarily mean that subcommand was passed, it just sets the "defaults". If a subcommand is set to `configurable` then passing the subcommand using `[sub]` in a configuration file will trigger the subcommand.)

CLI11 also supports configuration file in [TOML](https://github.com/toml-lang/toml) format.

```toml
# Comments are supported, using a #
# The default section is [default], case insensitive

value = 1
str = "A string"
vector = [1,2,3]

# Section map to subcommands
[subcommand]
in_subcommand = Wow
[subcommand.sub]
subcommand = true # could also be give as sub.subcommand=true
```

The main differences are in vector notation and comment character.  Note: CLI11 is not a full TOML parser as it just reads values as strings.  It is possible (but not recommended) to mix notation.

## Writing out a configure file

To print a configuration file from the passed arguments, use `.config_to_str(default_also=false, prefix="", write_description=false)`, where `default_also` will also show any defaulted arguments, `prefix` will add a prefix, and `write_description` will include option descriptions.

### Customization of configure file output
The default config parser/generator has some customization points that allow variations on the INI format.  The default formatter has a base configuration that matches the INI format.  It defines 5 characters that define how different aspects of the configuration are handled
```cpp
/// the character used for comments
char commentChar = ';';
/// the character used to start an array '\0' is a default to not use
char arrayStart = '\0';
/// the character used to end an array '\0' is a default to not use
char arrayEnd = '\0';
/// the character used to separate elements in an array
char arraySeparator = ' ';
/// the character used separate the name from the value
char valueDelimiter = '=';
```

These can be modified via setter functions

- ` ConfigBase *comment(char cchar)` Specify the character to start a comment block
-  `ConfigBase *arrayBounds(char aStart, char aEnd)`  Specify the start and end characters for an array
-  `ConfigBase *arrayDelimiter(char aSep)` Specify the delimiter character for an array
-  `ConfigBase *valueSeparator(char vSep)` Specify the delimiter between a name and value

For example to specify reading a configure file that used `:` to separate name and values

```cpp
auto config_base=app.get_config_formatter_base();
config_base->valueSeparator(':');
```

The default configuration file will read TOML files, but will write out files in the INI format.  To specify outputting TOML formatted files use
```cpp
app.config_formatter(std::make_shared<CLI::ConfigTOML>());
```

which makes use of a predefined modification of the ConfigBase class which INI also uses. If a custom formatter is used that is not inheriting from the from ConfigBase class `get_config_formatter_base() will return a nullptr if RTTI is on (usually the default), or garbage if RTTI is off, so some care must be exercised in its use with custom configurations.

## Custom formats

{% hint style='info' %}
New in CLI11 1.6
{% endhint %}

You can invent a custom format and set that instead of the default INI formatter. You need to inherit from `CLI::Config` and implement the following two functions:

```cpp
std::string to_config(const CLI::App *app, bool default_also, bool, std::string) const;
std::vector<CLI::ConfigItem> from_config(std::istream &input) const;
```

The `CLI::ConfigItem`s that you return are simple structures with a name, a vector of parents, and a vector of results. A optionally customizable `to_flag` method on the formatter lets you change what happens when a ConfigItem turns into a flag.

Finally, set your new class as new config formatter:

```cpp
app.config_formatter(std::make_shared<NewConfig>());
```

See [`examples/json.cpp`](https://github.com/CLIUtils/CLI11/blob/master/examples/json.cpp) for a complete JSON config example.


## Triggering Subcommands
Configuration files can be used to trigger subcommands if a subcommand is set to configure.  By default configuration file just set the default values of a subcommand.  But if the `configure()` option is set on a subcommand then the if the subcommand is utilized via a `[subname]` block in the configuration file it will act as if it were called from the command line.  Subsubcommands can be triggered via [subname.subsubname].  Using the `[[subname]]` will be as if the subcommand were triggered multiple times from the command line.  This functionality can allow the configuration file to act as a scripting file.

For custom configuration files this behavior can be triggered by specifying the parent subcommands in the structure and `++` as the name to open a new subcommand scope and `--` to close it.  These names trigger the different callbacks of configurable subcommands.
