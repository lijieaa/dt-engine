"""Build configuration for the industrial_runtime built-in module.

This module provides Godot-side bindings for the driver-engine industrial
data runtime: WebSocket protocol client, client-side tag cache, tag/property
binding core, expression evaluator, and data type conversions. All heavy
logic lives in C++ (PRD §6.3); GDScript in the companion project stays as
thin UI glue (PRD §6.4).
"""


def can_build(env, platform):
    # The module has no third-party deps and uses only built-in websocket +
    # core containers. Any desktop platform is fine.
    return platform in ("windows", "linuxbsd", "macos", "android", "ios", "web", "visionos")


def configure(env):
    pass


def get_doc_classes():
    return [
        "IndustrialRuntime",
        "IndustrialRuntimeHost",
        "TagCache",
        "WSClient",
        "TagBinding",
        "ExprEval",
    ]


def get_doc_path():
    return "doc_classes"
