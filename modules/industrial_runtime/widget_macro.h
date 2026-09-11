#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

/// Macro core: ordered steps { tag, value, delay_ms }.
/// execute(immediate: bool): immediate runs all steps synchronously and emits
/// step_done per step; non-immediate schedules the same synchronous run on the
/// next frame via a SceneTreeTimer (delay_ms is honored between steps).
/// All containers follow the module standard (Ruling W6): Vector/String, no STL.
class WidgetMacro : public RefCounted {
	GDCLASS(WidgetMacro, RefCounted);

protected:
	static void _bind_methods();

public:
	String get_version() const;

	/// Configure from: { steps: [ { tag: String, value: Variant,
	/// delay_ms: int }, ... ] }. Returns false on structural errors.
	bool configure(const Dictionary &p_cfg);

	/// Array of step dicts { tag, value, delay_ms }.
	Array get_steps() const;

	/// Execute the macro. immediate=true: run synchronously now.
	/// immediate=false: schedule a synchronous run on the next frame.
	/// Emits step_done(idx, tag, value) per step and finished(ok) at the end.
	bool execute(bool p_immediate);

	void clear();

	/// Callback invoked by the scheduled timer (GDScript heartbeat) to start
	/// the deferred run once.
	void tick_run();

	/// True while a run is in progress or scheduled.
	bool is_running() const;

private:
	struct Step {
		String tag;
		Variant value;
		int delay_ms = 0;
	};

	Vector<Step> steps;
	bool scheduled = false;
	bool running = false;

	void run_all();
};