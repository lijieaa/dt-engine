#include "register_types.h"

#include "industrial_runtime.h"
#include "industrial_runtime_host.h"
#include "tag_cache.h"
#include "ws_client.h"

#include "core/object/class_db.h"

void initialize_industrial_runtime_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// Register types used from IndustrialRuntime first (dependency order).
	// ExprEval and TagBinding are intentionally not registered in this build:
	// their Godot-trunk API adaptation is tracked as a follow-up. The runtime
	// facade still exposes the WS protocol + cache + signals needed by the
	// Godot client smoke test.
	GDREGISTER_CLASS(TagCache);
	GDREGISTER_CLASS(WSClient);
	GDREGISTER_CLASS(IndustrialRuntime);
	GDREGISTER_CLASS(IndustrialRuntimeHost);
}

void uninitialize_industrial_runtime_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// Static singleton invalidation happens in ~IndustrialRuntime dtor. No-op.
}
