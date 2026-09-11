"""Build configuration for the industrial_runtime built-in module.

Godot-side industrial data runtime: WebSocket client, tag cache, runtime host,
HTTP client, and HMI Tag* widgets (shared by editor, desktop export, and web).
"""


def can_build(env, platform):
	# No third-party deps; websocket + core only.
	return platform in ("windows", "linuxbsd", "macos", "android", "ios", "web", "visionos")


def configure(env):
	pass


def get_doc_classes():
	return [
		"IndustrialRuntime",
		"IndustrialRuntimeHost",
		"IndustrialRuntimeClient",
		"TagCache",
		"WSClient",
		"TagBinding",
		"ExprEval",
		"WidgetFormat",
		"InputSession",
		"InputSessionManager",
		"KeypadView",
		"KeypadActionButton",
		"KeypadHost",
		"KeypadRegistry",
		"TscnKeypadBackend",
		"TagNumInput",
		"TagNumKeypad",
		"TagAsciiInput",
		"TagAsciiKeypad",
		"TagLabel",
	]


def get_doc_path():
	return "doc_classes"
