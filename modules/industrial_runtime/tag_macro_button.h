#pragma once

#include "scene/gui/button.h"

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class WidgetMacro;

/// TagMacroButton - macro execution button. On press, runs the C++ WidgetMacro
/// core (execute), receives step_done/finished signals, writes each step's tag
/// through Runtime, and updates button text while busy.
class TagMacroButton : public Button {
	GDCLASS(TagMacroButton, Button);

protected:
	static void _bind_methods();
	void _notification(int p_what);

	void _on_step_done(int idx, const String &tag, const Variant &v);
	void _on_finished(bool ok);
	void _on_pressed();

public:
	TagMacroButton();
	~TagMacroButton();

	void set_macro_cfg(const Dictionary &p_cfg);
	Dictionary get_macro_cfg() const { return macro_cfg; }
	void set_idle_text(const String &t) { idle_text = t; set_text(t); }
	String get_idle_text() const { return idle_text; }
	void set_running_text(const String &t) { running_text = t; }
	String get_running_text() const { return running_text; }

	bool is_busy() const { return busy; }
	int get_done_count() const { return done_count; }

	/// Public entry point: execute the macro now (idempotent while busy).
	void execute();

private:
	Dictionary macro_cfg;
	String idle_text = "Run";
	String running_text = "Running...";

	bool busy = false;
	int done_count = 0;
	Ref<WidgetMacro> macro_core;

	int total_steps() const;
};
