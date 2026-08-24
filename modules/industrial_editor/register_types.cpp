#include "register_types.h"

#include "industrial_project.h"
#include "industrial_runtime_client.h"
#include "widget_alarm.h"
#include "widget_format.h"
#include "widget_macro.h"
#include "widget_meter.h"
#include "widget_recipe.h"
#include "widget_trend.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"

#ifdef TOOLS_ENABLED
#  include "industrial_editor_plugin.h"
#  include "editor/plugins/editor_plugin.h"
#  include "editor/settings/editor_settings.h"
#endif

void initialize_industrial_editor_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		// Layer 3 - ProjectSetting: "industrial/runtime/url"
		// 注册在 CORE 层（在 ProjectSettings 初始化之后、SCENE 之前），
		// 使 editor + template + exported runtime 三类 target 都能识别
		// 该 setting 并在 project.godot 中持久化 / 随包导出。
		ProjectSettings *ps = ProjectSettings::get_singleton();
		if (ps != nullptr) {
			const String key = IndustrialRuntimeClient::kProjectSettingKey;
			if (!ps->has_setting(key)) {
				ps->set_setting(key, IndustrialRuntimeClient::kDefaultUrl);
			}
			// Always set initial value so Inspector / Project Settings dialog
			// treats "factory default" as the known constant.
			ps->set_initial_value(key, String(IndustrialRuntimeClient::kDefaultUrl));
			// Add property metadata: plain STRING with placeholder hint text.
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
#  ifdef TOOLS_ENABLED
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
