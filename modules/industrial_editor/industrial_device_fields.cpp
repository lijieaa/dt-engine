#include "industrial_device_fields.h"

#include "editor/editor_string_names.h"
#include "editor/translations/editor_translation.h"
#include "core/object/callable_mp.h"
#include "scene/gui/check_button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/spin_box.h"

namespace {

bool key_in_list(const String &p_key, const char *const *p_keys, int p_count) {
	for (int i = 0; i < p_count; i++) {
		if (p_key == p_keys[i]) {
			return true;
		}
	}
	return false;
}

String label_for_field(const IndustrialFieldDef &p_field, bool p_translate) {
	if (p_translate) {
		return atr(p_field.name) + ":";
	}
	return p_field.name + ":";
}

} // namespace

IndustrialDeviceFieldGroup industrial_classify_device_field(const String &p_key) {
	static const char *kCommon[] = {
		"dev_type",
		"location_mode",
		"remote_hmi_ip",
	};
	static const char *kEth[] = {
		"interface_type",
		"ip",
		"port",
		"use_udp",
	};
	static const char *kSerial[] = {
		"serial_port",
		"baud_rate",
		"data_bits",
		"parity",
		"stop_bits",
		"flow_control",
		"station_no",
		"broadcast_station_no",
		"use_station_variable",
	};
	static const char *kTuning[] = {
		"poll_interval",
		"timeout",
		"comm_delay",
		"retries",
		"max_read_words",
		"max_write_words",
		"block_size_words",
	};

	if (key_in_list(p_key, kCommon, sizeof(kCommon) / sizeof(kCommon[0]))) {
		return IND_DEVICE_GROUP_COMMON;
	}
	if (key_in_list(p_key, kEth, sizeof(kEth) / sizeof(kEth[0])) ||
			key_in_list(p_key, kSerial, sizeof(kSerial) / sizeof(kSerial[0]))) {
		return IND_DEVICE_GROUP_INTERFACE;
	}
	if (key_in_list(p_key, kTuning, sizeof(kTuning) / sizeof(kTuning[0]))) {
		return IND_DEVICE_GROUP_TUNING;
	}
	return IND_DEVICE_GROUP_PROTOCOL;
}

bool industrial_is_known_device_flat_key(const String &p_key) {
	return industrial_classify_device_field(p_key) != IND_DEVICE_GROUP_PROTOCOL;
}

void industrial_add_param_row(VBoxContainer *p_container, const IndustrialFieldDef &p_field,
		const Variant &p_value, HashMap<String, Control *> &r_widgets, bool p_translate_label) {
	if (!p_container) {
		return;
	}

	HBoxContainer *row = memnew(HBoxContainer);

	Label *lbl = memnew(Label);
	lbl->set_text(label_for_field(p_field, p_translate_label));
	lbl->set_custom_minimum_size(Size2(160, 0));
	if (!p_field.tooltip.is_empty()) {
		lbl->set_tooltip_text(p_translate_label ? atr(p_field.tooltip) : p_field.tooltip);
	}
	row->add_child(lbl);

	Control *widget = nullptr;
	const Variant initial = p_value.get_type() == Variant::NIL ? p_field.default_value : p_value;

	switch (p_field.data_type) {
		case 0: { // int
			SpinBox *sb = memnew(SpinBox);
			sb->set_min(p_field.min_value);
			sb->set_max(p_field.max_value > 0 ? p_field.max_value : 999999);
			sb->set_step(1);
			sb->set_value((double)initial);
			sb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			widget = sb;
		} break;
		case 1: { // float
			SpinBox *sb = memnew(SpinBox);
			sb->set_min(p_field.min_value);
			sb->set_max(p_field.max_value > 0 ? p_field.max_value : 999999.0);
			sb->set_step(0.01);
			sb->set_value((double)initial);
			sb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			widget = sb;
		} break;
		case 2: { // string
			LineEdit *le = memnew(LineEdit);
			le->set_text(initial);
			if (p_field.max_length > 0) {
				le->set_max_length(p_field.max_length);
			}
			le->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			widget = le;
		} break;
		case 3: { // bool
			CheckButton *cb = memnew(CheckButton);
			cb->set_pressed(initial);
			widget = cb;
		} break;
		case 4: { // choice
			OptionButton *ob = memnew(OptionButton);
			for (int ci = 0; ci < p_field.choices.size(); ci++) {
				ob->add_item(p_field.choices[ci]);
			}
			const String current_val = initial;
			for (int ci = 0; ci < p_field.choices.size(); ci++) {
				if (p_field.choices[ci] == current_val) {
					ob->select(ci);
					break;
				}
			}
			ob->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			widget = ob;
		} break;
		default:
			break;
	}

	if (widget) {
		row->add_child(widget);
		r_widgets[p_field.key] = widget;
	}

	p_container->add_child(row);
}

Variant industrial_read_param_widget(Control *p_widget, int p_data_type) {
	if (!p_widget) {
		return Variant();
	}
	switch (p_data_type) {
		case 0:
		case 1:
			if (SpinBox *sb = Object::cast_to<SpinBox>(p_widget)) {
				return sb->get_value();
			}
			break;
		case 2:
			if (LineEdit *le = Object::cast_to<LineEdit>(p_widget)) {
				return le->get_text();
			}
			break;
		case 3:
			if (CheckButton *cb = Object::cast_to<CheckButton>(p_widget)) {
				return cb->is_pressed();
			}
			break;
		case 4:
			if (OptionButton *ob = Object::cast_to<OptionButton>(p_widget)) {
				if (ob->get_selected() >= 0) {
					return ob->get_item_text(ob->get_selected());
				}
				return String();
			}
			break;
		default:
			break;
	}
	return Variant();
}

Vector<IndustrialFieldDef> industrial_filter_driver_fields(const Vector<IndustrialFieldDef> &p_fields,
		const char *const *p_hidden_keys, int p_hidden_count) {
	HashMap<String, int> last_index;
	for (int i = 0; i < p_fields.size(); i++) {
		last_index[p_fields[i].key] = i;
	}

	Vector<IndustrialFieldDef> visible;
	for (int i = 0; i < p_fields.size(); i++) {
		if (last_index[p_fields[i].key] != i) {
			continue;
		}
		if (key_in_list(p_fields[i].key, p_hidden_keys, p_hidden_count)) {
			continue;
		}
		visible.push_back(p_fields[i]);
	}
	return visible;
}

void industrial_collect_param_widgets(const HashMap<String, Control *> &p_widgets, Dictionary &r_params) {
	for (const KeyValue<String, Control *> &kv : p_widgets) {
		const String &key = kv.key;
		Control *widget = kv.value;
		if (!widget) {
			continue;
		}
		if (SpinBox *sb = Object::cast_to<SpinBox>(widget)) {
			r_params[key] = sb->get_value();
		} else if (LineEdit *le = Object::cast_to<LineEdit>(widget)) {
			r_params[key] = le->get_text();
		} else if (CheckButton *cb = Object::cast_to<CheckButton>(widget)) {
			r_params[key] = cb->is_pressed();
		} else if (OptionButton *ob = Object::cast_to<OptionButton>(widget)) {
			if (ob->get_selected() >= 0) {
				r_params[key] = ob->get_item_text(ob->get_selected());
			} else {
				r_params[key] = String();
			}
		}
	}
}

void industrial_backfill_param_defaults(int p_driver_idx, Dictionary &r_params,
		const char *const *p_skip_keys, int p_skip_count) {
	Vector<IndustrialFieldDef> all_fields = industrial_get_driver_fields(p_driver_idx);
	HashMap<String, int> last_idx;
	for (int i = 0; i < all_fields.size(); i++) {
		last_idx[all_fields[i].key] = i;
	}
	for (int i = 0; i < all_fields.size(); i++) {
		if (last_idx[all_fields[i].key] != i) {
			continue;
		}
		const IndustrialFieldDef &f = all_fields[i];
		if (key_in_list(f.key, p_skip_keys, p_skip_count)) {
			continue;
		}
		if (r_params.has(f.key)) {
			continue;
		}
		switch (f.data_type) {
			case 0:
				r_params[f.key] = (int64_t)(double)f.default_value;
				break;
			case 1:
				r_params[f.key] = (double)f.default_value;
				break;
			case 2:
			case 4:
				r_params[f.key] = f.default_value;
				break;
			case 3:
				r_params[f.key] = (bool)f.default_value;
				break;
			default:
				break;
		}
	}
}
