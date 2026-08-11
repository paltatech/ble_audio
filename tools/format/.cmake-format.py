# Reference:
# https://github.com/black-desk/.format/blob/86250216b60370fa08310d80fb8d4ef3d9b674f0/.cmake-format.py

# ----------------------------------
# Options affecting listfile parsing
# ----------------------------------
#
with section("parse"):
    # Specify structure for custom cmake functions
    additional_commands = {
        "target_compile_features": {
            "kwargs": {"INTERFACE": "+", "PRIVATE": "+", "PUBLIC": "+"},
            "pargs": 1,
        },
        "target_include_directories": {
            "flags": ["SYSTEM", "BEFORE", "AFTER"],
            "kwargs": {"INTERFACE": "+", "PRIVATE": "+", "PUBLIC": "+"},
            "pargs": 1,
        },
        "target_link_libraries": {
            "kwargs": {"INTERFACE": "+", "PRIVATE": "+", "PUBLIC": "+"},
            "pargs": 1,
        },
        "target_sources": {
            "kwargs": {"INTERFACE": "+", "PRIVATE": "+", "PUBLIC": "+"},
            "pargs": 1,
        },
        "find_package": {
            "kwargs": {"NAMES": "+", "COMPONENTS": "+", "ORIGINAL_COMPONENTS": "+"},
            "flags": [
                "REQUIRED",
                "EXACT",
                "QUITE",
                "MODULE",
                "GLOBAL",
                "NO_POLICY_SCOPE",
                "BYPASS_PROVIDER",
            ],
        },
        "set_source_files_properties": {
            "kwargs": {"PROPERTIES": "+", "DIRECTORY": "+", "TARGET_DIRECTORY": "+"},
            "pargs": "*",
        },
    }


# -----------------------------
# Options affecting formatting.
# -----------------------------
with section("format"):
    line_width = 80

    # If a statement is wrapped to more than one line, than dangle the closing
    # parenthesis on it's own line
    dangle_parens = True

    max_pargs_hwrap= 1

    always_wrap = [
        "project",
        "find_package",
        "target_sources",
        "target_link_libraries",
        "target_include_directories",
        "set_source_files_properties",
    ]
