load("@bazel_skylib//rules:common_settings.bzl", "string_flag", skylib_bool_flag = "bool_flag")
load("@bazel_skylib//lib:selects.bzl", "selects")

def bool_flag(name, default):
    """Create a boolean flag and two config_settings with the names: <name>_true, <name>_false.

    bool_flag is a Bazel Macro that defines a boolean flag with the given name two config_settings,
    one for True, one for False. Reminder that Bazel has special syntax for unsetting boolean flags,
    but this does not work well with aliases.
    https://docs.bazel.build/versions/main/skylark/config.html#using-build-settings-on-the-command-line
    Thus it is best to define both an "enabled" alias and a "disabled" alias.

    Args:
        name: string, the name of the flag to create and use for the config_settings
        default: boolean, if the flag should default to on or off.
    """

    skylib_bool_flag(name = name, build_setting_default = default)

    native.config_setting(
        name = name + "_true",
        flag_values = {
            ":" + name: "True",
        },
        visibility = ["//visibility:public"],
    )

    native.config_setting(
        name = name + "_false",
        flag_values = {
            ":" + name: "False",
        },
        visibility = ["//visibility:public"],
    )

def os_flag():
    """Creates the 'os' string flag that specifies the OS to target, and a pair of
    'is_<os>_true' and 'is_<os>_false' targets.

    The OS flag can be specified on the command line with '--//src/tint:os=<os>'
    """

    OSes = [
        "win",
        "linux",
        "mac",
        "other"
    ]

    string_flag(
        name = "os",
        build_setting_default = "other",
        values = OSes,
    )

    for os in OSes:
        native.config_setting(
            name = "is_{}_true".format(os),
            flag_values = { ":os": os },
            visibility = ["//visibility:public"],
        )
        selects.config_setting_group(
            name = "is_{}_false".format(os),
            match_any = [ "is_{}_true".format(other) for other in OSes if other != os],
            visibility = ["//visibility:public"],
        )

