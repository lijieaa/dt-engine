#include "tag_macro_button.h"

#include "tag_widget_util.h"
#include "widget_macro.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "scene/main/node.h"
#include "scene/scene_string_names.h"

TagMacroButton::TagMacroButton() {
	macro_core.instantiate();
}

TagMacroButton::~TagMacroButton() {}

void TagMacroButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_macro_cfg", "cfg"), &TagMacroButton::set_macro_cfg);
	ClassDB::bind_method(D_METHOD("get_macro_cfg"), &TagMacroButton::get_macro_cfg);
	ClassDB::bind_method(D_METHOD("set_idle_text", "t"), &TagMacroButton::set_idle_text);
	ClassDB::bind_method(D_METHOD("get_idle_text"), &TagMacroButton::get_idle_text);
	ClassDB::bind_method(D_METHOD("set_running_text", "t"), &TagMacroButton::set_running_text);
	ClassDB::bind_method(D_METHOD("get_running_text"), &TagMacroButton::get_running_text);
	ClassDB::bind_method(D_METHOD("is_busy"), &TagMacroButton::is_busy);
	ClassDB::bind_method(D_METHOD("get_done_count"), &TagMacroButton::get_done_count);
	ClassDB::bind_method(D_METHOD("execute"), &TagMacroButton::execute);
	ClassDB::bind_method(D_METHOD("_on_step_done", "idx", "tag", "v"), &TagMacroButton::_on_step_done);
	ClassDB::bind_method(D_METHOD("_on_finished", "ok"), &TagMacroButton::_on_finished);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "macro_cfg"), "set_macro_cfg", "get_macro_cfg");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "idle_text"), "set_idle_text", "get_idle_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "running_text"), "set_running_text", "get_running_text");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "busy"), "", "is_busy");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "done_count"), "", "get_done_count");
}

void TagMacroButton::set_macro_cfg(const Dictionary &p_cfg) {
	macro_cfg = p_cfg;
	if (macro_core.is_valid() && !macro_cfg.is_empty()) {
		macro_core->configure(macro_cfg);
		if (!macro_core->is_connected("step_done", callable_mp(this, &TagMacroButton::_on_step_done))) {
			macro_core->connect("step_done", callable_mp(this, &TagMacroButton::_on_step_done));
		}
		if (!macro_core->is_connected("finished", callable_mp(this, &TagMacroButton::_on_finished))) {
			macro_core->connect("finished", callable_mp(this, &TagMacroButton::_on_finished));
		}
	}
}

void TagMacroButton::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (get_text().is_empty()) {
			set_text(idle_text);
		}
		if (!macro_cfg.is_empty() && macro_core.is_valid()) {
			macro_core->configure(macro_cfg);
			if (!macro_core->is_connected("step_done", callable_mp(this, &TagMacroButton::_on_step_done))) {
				macro_core->connect("step_done", callable_mp(this, &TagMacroButton::_on_step_done));
			}
			if (!macro_core->is_connected("finished", callable_mp(this, &TagMacroButton::_on_finished))) {
				macro_core->connect("finished", callable_mp(this, &TagMacroButton::_on_finished));
			}
		}
		// Hook Button.pressed -> execute.
		if (!is_connected(SceneStringName(pressed), callable_mp(this, &TagMacroButton::_on_pressed))) {
			connect(SceneStringName(pressed), callable_mp(this, &TagMacroButton::_on_pressed));
		}
	}
}

int TagMacroButton::total_steps() const {
	if (macro_core.is_null()) {
		return 0;
	}
	return macro_core->get_steps().size();
}

void TagMacroButton::execute() {
	if (busy || macro_core.is_null()) {
		return;
	}
	busy = true;
	done_count = 0;
	set_text(running_text);
	macro_core->execute(true);
}

void TagMacroButton::_on_pressed() {
	execute();
}

void TagMacroButton::_on_step_done(int idx, const String &tag, const Variant &v) {
	done_count++;
	// Write each step back through the bridge (mock writes are synchronous).
	tag_widget::write_tag(this, tag, v, "macro:step" + String::num(idx));
	set_text(running_text + " (" + String::num(done_count) + "/" + String::num(total_steps()) + ")");
}

void TagMacroButton::_on_finished(bool ok) {
	busy = false;
	if (ok) {
		set_text("完成 (" + String::num(done_count) + ")");
	} else {
		set_text("失败");
	}
}