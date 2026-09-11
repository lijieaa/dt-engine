#include "register_types.h"

#include "industrial_runtime.h"
#include "industrial_runtime_client.h"
#include "industrial_runtime_host.h"
#include "input_session.h"
#include "input_session_manager.h"
#include "keypad_action.h"
#include "keypad_action_button.h"
#include "keypad_backend.h"
#include "keypad_host.h"
#include "keypad_registry.h"
#include "keypad_view.h"
#include "tscn_keypad_backend.h"
#include "tag_alarm_list.h"
#include "tag_ascii_input.h"
#include "tag_ascii_keypad.h"
#include "tag_bar.h"
#include "tag_cache.h"
#include "tag_gauge.h"
#include "tag_label.h"
#include "tag_lamp.h"
#include "tag_macro_button.h"
#include "tag_meter.h"
#include "tag_num_display.h"
#include "tag_num_input.h"
#include "tag_num_keypad.h"
#include "tag_recipe.h"
#include "tag_switch.h"
#include "tag_trend.h"
#include "widget_alarm.h"
#include "widget_format.h"
#include "widget_macro.h"
#include "widget_meter.h"
#include "widget_recipe.h"
#include "widget_trend.h"
#include "ws_client.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"

void initialize_industrial_runtime_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		ProjectSettings *ps = ProjectSettings::get_singleton();
		if (ps != nullptr) {
			const String key = IndustrialRuntimeClient::kProjectSettingKey;
			if (!ps->has_setting(key)) {
				ps->set_setting(key, IndustrialRuntimeClient::kDefaultUrl);
			}
			ps->set_initial_value(key, String(IndustrialRuntimeClient::kDefaultUrl));
			ps->set_custom_property_info(
					PropertyInfo(Variant::STRING,
							key,
							PROPERTY_HINT_PLACEHOLDER_TEXT,
							"http://host:port  (e.g. http://192.168.0.100:8080)"));
		}
		return;
	}

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(TagCache);
	GDREGISTER_CLASS(WSClient);
	GDREGISTER_CLASS(IndustrialRuntime);
	GDREGISTER_CLASS(IndustrialRuntimeHost);
	GDREGISTER_CLASS(IndustrialRuntimeClient);
	GDREGISTER_CLASS(InputSession);
	GDREGISTER_CLASS(InputSessionManager);
	GDREGISTER_CLASS(KeypadActionButton);
	GDREGISTER_CLASS(KeypadView);
	GDREGISTER_CLASS(KeypadHost);
	GDREGISTER_CLASS(KeypadRegistry);
	GDREGISTER_CLASS(TscnKeypadBackend);

	GDREGISTER_CLASS(WidgetFormat);
	GDREGISTER_CLASS(WidgetTrend);
	GDREGISTER_CLASS(WidgetAlarm);
	GDREGISTER_CLASS(WidgetMeter);
	GDREGISTER_CLASS(WidgetRecipe);
	GDREGISTER_CLASS(WidgetMacro);

	GDREGISTER_CLASS(TagLabel);
	GDREGISTER_CLASS(TagNumDisplay);
	GDREGISTER_CLASS(TagNumInput);
	GDREGISTER_CLASS(TagNumKeypad);
	GDREGISTER_CLASS(TagAsciiInput);
	GDREGISTER_CLASS(TagAsciiKeypad);
	GDREGISTER_CLASS(TagLamp);
	GDREGISTER_CLASS(TagSwitch);
	GDREGISTER_CLASS(TagMeter);
	GDREGISTER_CLASS(TagBar);
	GDREGISTER_CLASS(TagGauge);
	GDREGISTER_CLASS(TagTrend);
	GDREGISTER_CLASS(TagAlarmList);
	GDREGISTER_CLASS(TagRecipe);
	GDREGISTER_CLASS(TagMacroButton);
}

void uninitialize_industrial_runtime_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
