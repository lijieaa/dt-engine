#include "register_types.h"

#include "industrial_project.h"
#include "industrial_runtime_client.h"
#include "widget_alarm.h"
#include "widget_format.h"
#include "widget_macro.h"
#include "widget_meter.h"
#include "widget_recipe.h"
#include "widget_trend.h"
#include "tag_alarm_list.h"
#include "tag_bar.h"
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

#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"

#ifdef TOOLS_ENABLED
#  include "industrial_editor_plugin.h"
#  include "industrial_device_list_row.h"
#  include "industrial_tag_list_row.h"
#  include "editor/plugins/editor_plugin.h"
#  include "editor/settings/editor_settings.h"
#endif

void initialize_industrial_editor_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		// ProjectSetting "industrial/runtime/url" is the editor UI input.
		// On industrial project save it is written into industrial/project.json
		// as "runtime_url"; get_runtime_url() reads project.json (not this setting).
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

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(IndustrialProject);
		GDREGISTER_CLASS(IndustrialRuntimeClient);
		GDREGISTER_CLASS(WidgetFormat);
		GDREGISTER_CLASS(WidgetTrend);
		GDREGISTER_CLASS(WidgetAlarm);
		GDREGISTER_CLASS(WidgetMeter);
		GDREGISTER_CLASS(WidgetRecipe);
		GDREGISTER_CLASS(WidgetMacro);
		// Engine-wide HMI tag widgets (available in Create Node for every project).
		GDREGISTER_CLASS(TagLabel);
		GDREGISTER_CLASS(TagNumDisplay);
		GDREGISTER_CLASS(TagNumInput);
		GDREGISTER_CLASS(TagNumKeypad);
		GDREGISTER_CLASS(TagLamp);
		GDREGISTER_CLASS(TagSwitch);
		GDREGISTER_CLASS(TagMeter);
		GDREGISTER_CLASS(TagBar);
		GDREGISTER_CLASS(TagGauge);
		GDREGISTER_CLASS(TagTrend);
		GDREGISTER_CLASS(TagAlarmList);
		GDREGISTER_CLASS(TagRecipe);
		GDREGISTER_CLASS(TagMacroButton);
#  ifdef TOOLS_ENABLED
		GDREGISTER_CLASS(IndustrialDeviceListRow);
		GDREGISTER_CLASS(IndustrialTagListRow);
		GDREGISTER_CLASS(IndustrialEditorPlugin);
#  endif
		return;
	}
#  ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// Layer 4 - EditorSetting: "industrial/runtime/url"
		// 只存在于 editor builds（用户全局偏好，不随项目/导出迁移）。
		EditorSettings *es = EditorSettings::get_singleton();
		if (es != nullptr) {
			const String key = IndustrialRuntimeClient::kEditorSettingKey;
			if (!es->has_setting(key)) {
				es->set_setting(key, Variant(String(IndustrialRuntimeClient::kDefaultUrl)));
			}
		}

		EditorPlugins::add_by_type<IndustrialEditorPlugin>();
	}
#  endif
}

void uninitialize_industrial_editor_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
