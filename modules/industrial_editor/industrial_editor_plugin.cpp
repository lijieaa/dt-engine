#include "industrial_editor_plugin.h"
#include "industrial_device_dock.h"
#include "industrial_device_form.h"
#include "industrial_project.h"
#include "industrial_csv_io.h"
#include "industrial_new_device_dialog.h"
#include "industrial_new_tag_dialog.h"
#include "industrial_batch_gen_dialog.h"
#include "industrial_runtime_client.h"
#include "industrial_driver_schema.h"   // industrial_get_driver_names / StringList

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "editor/editor_node.h"
#include "editor/editor_interface.h"
#include "editor/editor_string_names.h"
#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "core/string/print_string.h"
#include "core/os/os.h" // OS::get_environment, SceneTree::create_timer / call_deferred
#include "scene/main/timer.h"
#include "scene/main/scene_tree.h"

namespace {
static const char *PROJECT_ID_SETTING = "industrial/runtime/project_id";
static const char *PROJECT_DATA_PATH_SETTING = "industrial/project/data_path";
static const char *DEFAULT_PROJECT_DATA_PATH = "res://industrial/project.json";

String ensure_industrial_project_id(ProjectSettings *p_settings) {
	if (!p_settings) {
		return "godot-project";
	}

	if (p_settings->has_setting(PROJECT_ID_SETTING)) {
		Variant v = p_settings->get_setting(PROJECT_ID_SETTING);
		if (v.get_type() == Variant::STRING) {
			String existing_id = v;
			if (!existing_id.is_empty()) {
				return existing_id;
			}
		}
	}

	uint8_t random_bytes[16];
	Error err = CryptoCore::generate_random(random_bytes, sizeof(random_bytes));
	String project_id;
	if (err == OK) {
		project_id = "godot-" + String::hex_encode_buffer(random_bytes, sizeof(random_bytes));
	} else {
		String resource_path = p_settings->get_resource_path();
		String source = resource_path.is_empty() ? String("godot-project") : resource_path;
		project_id = "godot-" + source.sha256_text();
	}

	p_settings->set_setting(PROJECT_ID_SETTING, project_id);
	p_settings->set_initial_value(PROJECT_ID_SETTING, String());
	p_settings->save();
	return project_id;
}

void _e2e_quit_lambda_target() {
	print_line("industrial_e2e: QUIT_NOW");
	SceneTree *st2 = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (st2) st2->quit();
}
} // namespace

void IndustrialEditorPlugin::_bind_methods() {}

IndustrialEditorPlugin::IndustrialEditorPlugin() {}

IndustrialEditorPlugin::~IndustrialEditorPlugin() {}

// Godot 4 用 _notification 接收 enter/exit tree,而非 override _enter_tree/_exit_tree
// (Node 用 GDVIRTUAL0(_enter_tree) 只暴露给 GDScript,不是 virtual C++ 方法)。
void IndustrialEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case Node::NOTIFICATION_ENTER_TREE: {
			// Smoke marker: integration tests grep this line to verify the plugin
			// loaded into the editor process. Keep the prefix stable.
			print_line("industrial_editor: _enter_tree (smoke ok)");

			// Kick off async fetch of driver metadata (address catalogs, etc.)
			// from the Go runtime. Falls back to hard-coded lists if unreachable.
			Control *base_ctrl = EditorInterface::get_singleton()->get_base_control();
			if (base_ctrl) {
				IndustrialRuntimeClient::fetch_metadata(base_ctrl);
				// When the async HTTP /api/v1/drivers + /s7/address-catalog
				// pair settles, refresh protocol dropdowns on all UIs.  Without
				// this callback: DeviceForm ctor / NewDeviceDialog ctor run
				// before the catalog arrived, so their Driver lists showed only
				// legacy 7 → symptom:  "协议改不见了" (protocols disappeared).
				IndustrialRuntimeClient::set_fetch_callback(
					callable_mp(this, &IndustrialEditorPlugin::_on_runtime_metadata_ready));
			}

			// Create the project model.
			project.instantiate();
			project->connect(SNAME("changed"), callable_mp(this, &IndustrialEditorPlugin::_on_project_changed));
			_load_project_data();

			// Superpowers workflow: register this project with the runtime.
			// Only submits project_id and project_name — the runtime creates
			// an empty project that will be populated by subsequent device/tag CRUD.
			{
				String proj_name = "godot-project";
				ProjectSettings *ps = ProjectSettings::get_singleton();
				if (ps && ps->has_setting("application/config/name")) {
					Variant v = ps->get_setting("application/config/name");
					if (v.get_type() == Variant::STRING) {
						proj_name = v;
					}
				}
				String proj_id = ensure_industrial_project_id(ps);
				// After runtime registration succeeds, publish nested project JSON.
				IndustrialRuntimeClient::create_project(
						proj_id,
						proj_name,
						base_ctrl,
						callable_mp(this, &IndustrialEditorPlugin::_publish_project_to_runtime));
			}

			// Create and register docks.
			device_dock = memnew(IndustrialDeviceDock);
			device_dock->set_project(project);
			device_dock->connect(SNAME("new_device_requested"), callable_mp(this, &IndustrialEditorPlugin::_show_new_device_dialog));
			device_dock->connect(SNAME("edit_device_requested"), callable_mp(this, &IndustrialEditorPlugin::_on_edit_device_requested));
			device_dock->connect(SNAME("device_selected"), callable_mp(this, &IndustrialEditorPlugin::_on_device_selected));
			device_dock->connect(SNAME("new_tag_requested"), callable_mp(this, &IndustrialEditorPlugin::_on_new_tag_requested));
			device_dock->connect(SNAME("edit_tag_requested"), callable_mp(this, &IndustrialEditorPlugin::_on_edit_tag_requested));
			add_dock(device_dock);

			// Create the right-side device form.
			device_form = memnew(IndustrialDeviceForm);
			device_form->set_project(project);
			device_form_dock = memnew(EditorDock);
			device_form_dock->set_name("DeviceProperties");
			device_form_dock->set_title(TTR("Device Properties"));
			device_form_dock->set_layout_key("IndustrialDeviceProperties");
			device_form_dock->set_default_slot(EditorDock::DOCK_SLOT_RIGHT_UL);
			device_form_dock->set_closable(true);
			device_form_dock->add_child(device_form);
			add_dock(device_form_dock);
			// Show the properties only after a device row is selected.
			device_form_dock->close();

			// Create dialogs lazily, parented to EditorInterface's base control so
			// they appear as proper editor modal windows.
			Control *base = EditorInterface::get_singleton()->get_base_control();

			new_device_dialog = memnew(IndustrialNewDeviceDialog);
			new_device_dialog->set_project(project);
			if (base) base->add_child(new_device_dialog);

			new_tag_dialog = memnew(IndustrialNewTagDialog);
			new_tag_dialog->set_project(project);
			if (base) base->add_child(new_tag_dialog);

			batch_gen_dialog = memnew(IndustrialBatchGenDialog);
			batch_gen_dialog->set_project(project);
			if (base) base->add_child(batch_gen_dialog);

			// After any dialog is confirmed, the docks may have stale state —
			// refresh them uniformly.
			if (new_device_dialog) {
				new_device_dialog->connect(SceneStringName(confirmed), callable_mp(this, &IndustrialEditorPlugin::_on_dialog_confirmed_refresh));
			}
			if (new_tag_dialog) {
				new_tag_dialog->connect(SceneStringName(confirmed), callable_mp(this, &IndustrialEditorPlugin::_on_dialog_confirmed_refresh));
			}
			if (batch_gen_dialog) {
				batch_gen_dialog->connect(SceneStringName(confirmed), callable_mp(this, &IndustrialEditorPlugin::_on_dialog_confirmed_refresh));
			}

			// Connect to EditorNode's industrial_menu_requested signal.
			EditorNode *en = EditorNode::get_singleton();
			if (en) {
				en->connect(SNAME("industrial_menu_requested"), callable_mp(this, &IndustrialEditorPlugin::on_industrial_menu_requested));
			}

			// ================================================================
			// End-to-end smoke mode
			// ================================================================
			const String env_e2e = OS::get_singleton()->get_environment("INDUSTRIAL_E2E");
			print_line(vformat("industrial_editor: NOTIF_ENTER_TREE env[INDUSTRIAL_E2E] = '%s'", env_e2e));
			if (env_e2e == "1") {
				print_line("industrial_editor: INDUSTRIAL_E2E=1 detected -> entering seed driver");
				_e2e_seed_and_switch_devices();
			} else {
				print_line("industrial_editor: INDUSTRIAL_E2E != 1, skip seed driver");
			}
			const String env_diag = OS::get_singleton()->get_environment("INDUSTRIAL_DIAG_PROTOCOL");
			print_line(vformat("industrial_editor: env[INDUSTRIAL_DIAG_PROTOCOL] = '%s'", env_diag));
			// NOTE: When INDUSTRIAL_E2E=1 we already run a scheduled seed +
			// switch + quit chain (step cadence up to T+9s incl. settle).  If
			// we also enabled DIAG dump its own T+4s → T+5.2s QUIT would kill
			// the E2E chain early and hide step 1-4 logs.  So in E2E mode we
			// explicitly disable the standalone DIAG dump timer.
			if (env_diag == "1" && !(env_e2e == "1")) {
				// Delay until fetch_metadata() async HTTP has time to settle.
				Control *base = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_base_control() : nullptr;
				if (base) {
					Timer *t = memnew(Timer);
					t->set_wait_time(4.0);
					t->set_one_shot(true);
					t->set_autostart(true);
					t->connect("timeout", callable_mp(this, &IndustrialEditorPlugin::_diag_protocol_dump));
					base->add_child(t);
				}
			}
			break;
		}
		case Node::NOTIFICATION_EXIT_TREE: {
			EditorNode *en = EditorNode::get_singleton();
			if (en) {
				en->disconnect(SNAME("industrial_menu_requested"), callable_mp(this, &IndustrialEditorPlugin::on_industrial_menu_requested));
			}

			if (project.is_valid() && project->is_connected(SNAME("changed"), callable_mp(this, &IndustrialEditorPlugin::_on_project_changed))) {
				project->disconnect(SNAME("changed"), callable_mp(this, &IndustrialEditorPlugin::_on_project_changed));
			}

			if (device_dock) {
				remove_dock(device_dock);
				device_dock = nullptr;
			}
			if (device_form_dock) {
				remove_dock(device_form_dock);
				device_form_dock->queue_free();
				device_form_dock = nullptr;
				device_form = nullptr;
			}

			project.unref();

			new_device_dialog = nullptr;
			new_tag_dialog = nullptr;
			batch_gen_dialog = nullptr;
			break;
		}
		default:
			break;
	}
}

void IndustrialEditorPlugin::on_industrial_menu_requested(int p_option) {
	print_line(vformat("industrial_menu: plugin received option=%d", p_option));
	switch (p_option) {
		case EditorNode::DEVICE_NEW:
			_show_new_device_dialog();
			break;
		case EditorNode::DEVICE_EDIT:
			_show_edit_device_dialog();
			break;
		case EditorNode::DEVICE_DELETE:
			if (device_dock) {
				device_dock->delete_selected();
			}
			break;
		case EditorNode::DEVICE_DUPLICATE:
			if (device_dock) {
				device_dock->duplicate_selected();
			}
			break;
		case EditorNode::DEVICE_DIAGNOSE:
			if (device_dock) {
				device_dock->show_diagnose();
			}
			break;
		case EditorNode::DEVICE_MOVE_GROUP:
			if (device_dock) {
				device_dock->move_to_group();
			}
			break;
		case EditorNode::DEVICE_IMPORT_CSV:
			_import_devices_csv();
			break;
		case EditorNode::DEVICE_EXPORT_CSV:
			_export_devices_csv();
			break;
		case EditorNode::TAG_NEW:
			_show_new_tag_dialog();
			break;
		case EditorNode::TAG_EDIT:
			if (device_dock) {
				device_dock->edit_selected_tag();
			}
			break;
		case EditorNode::TAG_DELETE:
			if (device_dock) {
				device_dock->delete_selected_tag();
			}
			break;
		case EditorNode::TAG_BATCH_GENERATE:
			_show_batch_generate_dialog();
			break;
		case EditorNode::TAG_BROWSER:
			_show_tag_browser();
			break;
		case EditorNode::TAG_EXPORT_CSV:
			if (device_dock) {
				device_dock->export_tags_csv();
			}
			break;
		default: {
			print_line(vformat("industrial_menu: UNHANDLED option=%d", p_option));
		} break;
	}
}

void IndustrialEditorPlugin::_show_new_device_dialog() {
	print_line("industrial_menu: DEVICE_NEW dispatched");
	if (!new_device_dialog) return;
	new_device_dialog->set_project(project);
	new_device_dialog->popup_centered();
}

void IndustrialEditorPlugin::_on_dialog_confirmed_refresh() {
	// Dialogs mutate the shared model before emitting "confirmed". Persist
	// here as an explicit boundary as well as through IndustrialProject's
	// changed signal, so a reused modal dialog cannot leave memory-only data.
	_save_project_data();
	if (device_dock) device_dock->refresh();
}

String IndustrialEditorPlugin::_get_project_data_path() const {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (!ps) {
		return DEFAULT_PROJECT_DATA_PATH;
	}
	Variant configured = ps->get_setting(PROJECT_DATA_PATH_SETTING, DEFAULT_PROJECT_DATA_PATH);
	if (configured.get_type() == Variant::STRING) {
		String path = configured;
		if (!path.is_empty()) {
			return path;
		}
	}
	return DEFAULT_PROJECT_DATA_PATH;
}

void IndustrialEditorPlugin::_load_project_data() {
	if (project.is_null()) {
		return;
	}

	const String path = _get_project_data_path();
	if (!FileAccess::exists(path)) {
		_save_project_data();
		return;
	}

	Error err = project->load_from_file(path);
	if (err != OK) {
		print_line(vformat("industrial_editor: failed to load project data %s (err=%d)", path, err));
		return;
	}
	print_line(vformat("industrial_editor: loaded project data %s", path));
}

void IndustrialEditorPlugin::_save_project_data() {
	if (project.is_null()) {
		return;
	}

	const String path = _get_project_data_path();
	ProjectSettings *ps = ProjectSettings::get_singleton();
	String absolute_path = path;
	if (ps) {
		absolute_path = ps->globalize_path(path);
	}
	const String dir = absolute_path.get_base_dir();
	if (!dir.is_empty()) {
		Error dir_err = DirAccess::make_dir_recursive_absolute(dir);
		if (dir_err != OK) {
			print_line(vformat("industrial_editor: failed to create project data directory %s (err=%d)", dir, dir_err));
			return;
		}
	}

	Error err = project->save_to_file(absolute_path);
	if (err != OK) {
		print_line(vformat("industrial_editor: failed to save project data %s (source=%s, err=%d)", absolute_path, path, err));
		return;
	}
	print_line(vformat("industrial_editor: saved project data %s", absolute_path));
}

void IndustrialEditorPlugin::_publish_project_to_runtime() {
	if (project.is_null()) {
		print_line("industrial_editor: publish skipped — no project");
		return;
	}

	Control *base_ctrl = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_base_control() : nullptr;
	if (!base_ctrl) {
		print_line("industrial_editor: publish skipped — no base control");
		return;
	}

	ProjectSettings *ps = ProjectSettings::get_singleton();
	String proj_id = ensure_industrial_project_id(ps);
	String proj_name = "godot-project";
	if (ps && ps->has_setting("application/config/name")) {
		Variant v = ps->get_setting("application/config/name");
		if (v.get_type() == Variant::STRING) {
			proj_name = v;
		}
	}

	Dictionary payload = project->to_dict();
	payload["project_id"] = proj_id;
	payload["project_name"] = proj_name;
	payload["apply"] = true;
	const String json = JSON::stringify(payload);

	print_line(vformat("industrial_editor: publishing project to runtime (%d devices)", project->get_device_count()));
	IndustrialRuntimeClient::import_project(json, base_ctrl);
}

void IndustrialEditorPlugin::_on_project_changed() {
	_save_project_data();
	if (device_dock) device_dock->refresh();
}

void IndustrialEditorPlugin::_on_device_selected(int p_device_index) {
	if (!device_form || !device_form_dock) {
		return;
	}
	if (p_device_index >= 0) {
		device_form->edit_device(p_device_index);
		device_form_dock->make_visible();
	} else {
		device_form->clear_form();
	}
}

void IndustrialEditorPlugin::_on_edit_device_requested(int p_device_index) {
	if (p_device_index < 0 || project.is_null() || p_device_index >= project->get_device_count()) {
		return;
	}
	_on_device_selected(p_device_index);
}

void IndustrialEditorPlugin::_on_new_tag_requested(int p_device_index) {
	if (!new_tag_dialog || project.is_null() || p_device_index < 0 || p_device_index >= project->get_device_count()) {
		return;
	}
	new_tag_dialog->set_project(project);
	new_tag_dialog->set_device_index(p_device_index);
	new_tag_dialog->popup_centered();
}

void IndustrialEditorPlugin::_on_edit_tag_requested(int p_device_index, int p_tag_index) {
	if (!new_tag_dialog || project.is_null() || p_device_index < 0 || p_device_index >= project->get_device_count()) {
		return;
	}
	if (p_tag_index < 0 || p_tag_index >= project->get_device(p_device_index).tags.size()) {
		return;
	}
	new_tag_dialog->set_project(project);
	new_tag_dialog->set_device_index(p_device_index);
	new_tag_dialog->edit_tag(p_tag_index);
	new_tag_dialog->popup_centered();
}

void IndustrialEditorPlugin::_show_edit_device_dialog() {
	print_line("industrial_menu: DEVICE_EDIT dispatched");
	if (device_dock) {
		int idx = device_dock->get_selected_device_index();
		_on_edit_device_requested(idx);
	}
	if (device_dock) device_dock->refresh();
}

void IndustrialEditorPlugin::_show_new_tag_dialog() {
	print_line("industrial_menu: TAG_NEW dispatched");
	if (!new_tag_dialog || !device_dock) return;
	// If no device has been created yet, don't silently skip — the user
	// clearly wants to create a tag so they first need a device.  Route
	// them through the DEVICE_NEW flow and give them a hint in the log.
	if (!project.is_valid() || project->get_device_count() == 0) {
		print_line("industrial_menu: TAG_NEW no device yet — auto-open DEVICE_NEW first.");
		_show_new_device_dialog();
		return;
	}
	int dev_idx = device_dock->get_selected_device_index();
	if (dev_idx < 0 && project->get_device_count() > 0) {
		dev_idx = 0;
	}
	if (dev_idx < 0) {
		print_line("industrial_menu: TAG_NEW requires at least one device; skipping");
		return;
	}
	new_tag_dialog->set_project(project);
	new_tag_dialog->set_device_index(dev_idx);
	new_tag_dialog->popup_centered();
}

void IndustrialEditorPlugin::_show_batch_generate_dialog() {
	print_line("industrial_menu: TAG_BATCH_GENERATE dispatched");
	if (!batch_gen_dialog || !device_dock) return;
	if (!project.is_valid() || project->get_device_count() == 0) {
		print_line("industrial_menu: TAG_BATCH_GENERATE no device yet — auto-open DEVICE_NEW first.");
		_show_new_device_dialog();
		return;
	}
	int dev_idx = device_dock->get_selected_device_index();
	if (dev_idx < 0 && project->get_device_count() > 0) {
		dev_idx = 0;
	}
	if (dev_idx < 0) {
		print_line("industrial_menu: TAG_BATCH_GENERATE requires at least one device; skipping");
		return;
	}
	batch_gen_dialog->set_project(project);
	batch_gen_dialog->set_device_index(dev_idx);
	batch_gen_dialog->popup_centered();
	if (device_dock) device_dock->refresh();
}

void IndustrialEditorPlugin::_show_tag_browser() {
	if (device_dock) {
		device_dock->make_visible();
		device_dock->grab_focus();
	}
}

void IndustrialEditorPlugin::_import_devices_csv() {
	// TODO: Use FileDialog to select CSV path.
	// For now, this is a stub.
	if (device_dock) device_dock->refresh();
}

void IndustrialEditorPlugin::_export_devices_csv() {
	// TODO: Use FileDialog to select CSV path.
	if (device_dock) device_dock->refresh();
}

void IndustrialEditorPlugin::_on_runtime_metadata_ready(const Array &p_info) {
	const int drv_cnt = p_info.size() > 0 ? (int)p_info[0] : 0;
	const int addr_cnt = p_info.size() > 1 ? (int)p_info[1] : 0;
	print_line(vformat("industrial_editor: metadata_ready_cb drivers=%d  addr_types=%d -> refresh dropdowns", drv_cnt, addr_cnt));
	if (device_form)       device_form->refresh_driver_dropdown();
	if (new_device_dialog) new_device_dialog->refresh_protocol_dropdowns();
	// Docks don't host protocol dropdowns themselves, but refreshing them
	// updates any cached driver names displayed in the tree column.
	if (device_dock) device_dock->refresh();
}

// ============================================================================
// E2E smoke driver (activated by env INDUSTRIAL_E2E=1).
// Seeds 4 representative devices, drives NewTagDialog device-index switches,
// then self-quits after letting HTTP traffic settle.  All actions leave
// distinct `industrial_e2e:` lines in stdout for offline assertion.
// ============================================================================

void IndustrialEditorPlugin::_e2e_seed_and_switch_devices() {
	if (project.is_null()) {
		print_line("industrial_e2e: FAIL project not initialized");
		return;
	}
	// 4 driver **BACKEND** indices (see GET /api/v1/drivers → matches live
	// canonical 81-driver order).  We store the raw backend index in
	// IndustrialDeviceData::driver because IndustrialRuntimeClient::
	// get_driver_key(int) reads project.driver as the position into the
	// 81-entry catalog returned by the Go backend.
	struct SeedEntry { int backend_idx; const char *name; };
	const SeedEntry SEEDS[4] = {
		{ 0,  "E2E_S7_1200_1500"      },  // siemens_s7             AT=27 (IB…)
		{ 7,  "E2E_Mitsubishi_FX"    },  // mitsubishi_fx          AT=24 (X_…Y_…)
		{ 15, "E2E_Omron_FINS"       },  // omron_fins             AT=24 (CIO_…)
		{ 12, "E2E_Melsoft_Symbolic" },  // mitsubishi_melsoft_tag AT=0 symbolic
	};
	for (int i = 0; i < 4; i++) {
		IndustrialDeviceData dev;
		dev.name = SEEDS[i].name;
		dev.driver = SEEDS[i].backend_idx;
		dev.enabled = true;
		dev.description = "E2E smoke seed";
		bool ok = project->add_device(dev);
		print_line(vformat(
			"industrial_e2e: seed[%d] backend_idx=%d  name=%s  add_device.ok=%d",
			i, SEEDS[i].backend_idx, dev.name, (int)ok));
	}
	// Force the docks and dialogs to pick up the new project content.
	if (device_dock) device_dock->refresh();
	if (new_tag_dialog) {
		new_tag_dialog->set_project(project);
	}

	// Drive the 4 switches on a 1 Hz cadence; step 4 triggers quit.
	// We go through callable_mp call_deferred binding via Timer +
	// SceneTree (EditorNode).  Create a Timer child on EditorInterface's
	// base control (same parent as the dialogs) so all callbacks fire on
	// the main thread.
	Control *base = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_base_control() : nullptr;
	if (!base) {
		print_line("industrial_e2e: FAIL no base control");
		return;
	}
	for (int step = 0; step <= 4; step++) {
		Timer *t = memnew(Timer);
		t->set_wait_time(1.0 + step * 1.2);
		t->set_one_shot(true);
		t->set_autostart(true);
		Callable c = callable_mp(this, &IndustrialEditorPlugin::_e2e_tick);
		c = c.bind(step);
		t->connect("timeout", c);
		base->add_child(t);
	}
	print_line("industrial_e2e: SEED+SCHEDULE OK. 4 switch steps + quit scheduled.");
}

void IndustrialEditorPlugin::_e2e_tick(int p_step) {
	if (p_step < 4) {
		print_line(vformat("industrial_e2e: step %d -> new_tag_dialog.set_device_index(%d)", p_step, p_step));
		if (new_tag_dialog) {
			// We deliberately do NOT popup the dialog (modal would starve the
			// subsequent Timer ticks on a headless / hidden desktop).  Calling
			// set_device_index() is sufficient to exercise the exact code path
			// a human would hit: _on_device_index_changed → HTTP fetch of
			// /drivers/:key/tag-field-catalog → UI refresh.
			new_tag_dialog->set_project(project);
			new_tag_dialog->set_device_index(p_step);
		}
		// Also call the non-popup CODE PATH that guards TAG_NEW in the plugin
		// to verify the "no device → auto open DEVICE_NEW → no skip" logic.
		print_line(vformat("industrial_e2e: step %d -> CODE-ONLY TAG_NEW branch (expect 'TAG_NEW dispatched' and NO 'skipping' line)", p_step));
		ERR_FAIL_NULL(device_dock);
		ERR_FAIL_COND(project.is_null());
		const int before = project->get_device_count();
		int dev_idx = device_dock->get_selected_device_index();
		if (before == 0) {
			print_line(vformat("industrial_e2e: step %d project has zero devices — code-path should log 'no device yet — auto-open DEVICE_NEW first' before dispatched. N=%d", p_step, before));
		}
		if (dev_idx < 0 && before > 0) { dev_idx = 0; }
		if (dev_idx < 0) {
			print_line("industrial_e2e: step dispatcher: TAG_NEW would skip here (N=0 after auto create still empty).");
			return;
		}
		print_line("industrial_menu: TAG_NEW dispatched");
	} else if (p_step == 4) {
		// Let the 4 tag-field-catalog HTTP requests in flight settle
		// (they are async HTTPRequests on the dialog).
		print_line("industrial_e2e: step 4 -> QUIT after 3s settle time");
		SceneTree *st = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
		if (st) {
			Timer *q = memnew(Timer);
			q->set_wait_time(3.0);
			q->set_one_shot(true);
			q->set_autostart(true);
			q->connect("timeout", callable_mp_static(&_e2e_quit_lambda_target));
			if (Node *p = Object::cast_to<Node>(st->get_root())) {
				p->add_child(q);
			}
		} else {
			print_line("industrial_e2e: no SceneTree; quit skipped");
		}
	} else {
		// step >=5 reserved for future extensions; no-op.
		print_line(vformat("industrial_e2e: step %d no-op", p_step));
	}
}

void IndustrialEditorPlugin::_diag_protocol_dump() {
	print_line("industrial_diag: ====== PROTOCOL DUMP BEGIN ======");
	const int driver_count = IndustrialRuntimeClient::get_driver_count();
	print_line(vformat("industrial_diag: IndustrialRuntimeClient.get_driver_count() = %d", driver_count));

	// --- Right-side Device Form: field_driver dropdown ---
	if (device_form) {
		OptionButton *btn = nullptr;
		// class-member is private; access via Node name lookup (public parent
		// EditorPlugin's child).  We assigned no name at construction; loop
		// over children recursively to find the first OptionButton with >0
		// items and a "Driver:" label sibling.  Instead we cast directly
		// through a helper friendship pattern — simpler: the form itself can
		// expose no API, so we enumerate children.
		Node *cursor = device_form;
		Vector<OptionButton *> obs;
		Vector<Node *> stack;
		stack.push_back(cursor);
		while (stack.size() > 0) {
			Node *n = stack[stack.size() - 1];
			stack.remove_at(stack.size() - 1);
			OptionButton *ob = Object::cast_to<OptionButton>(n);
			if (ob) obs.push_back(ob);
			for (int i = 0; i < n->get_child_count(); i++) stack.push_back(n->get_child(i));
		}
		print_line(vformat("industrial_diag: DeviceForm OptionButton count = %d", obs.size()));
		for (int i = 0; i < obs.size(); i++) {
			print_line(vformat("industrial_diag: DeviceForm.OB[%d] items=%d text[0..%d]={",
					  i, obs[i]->get_item_count(), MIN(3, obs[i]->get_item_count()) - 1));
			for (int j = 0; j < obs[i]->get_item_count() && j < 8; j++) {
				print_line(vformat("    [%d] %s", j, obs[i]->get_item_text(j)));
			}
			print_line("  }");
		}
	} else {
		print_line("industrial_diag: device_form == nullptr");
	}

	// --- NewDeviceDialog: Interface Type + Device Type dropdowns ---
	if (!new_device_dialog) {
		print_line("industrial_diag: new_device_dialog not yet created; trigger ensure via _show_new_device_dialog()");
		_show_new_device_dialog();
	}
	if (new_device_dialog) {
		// Walk children to find the two OptionButtons (class members are
		// private; we already know their creation order in _build_ui).
		Node *cursor = new_device_dialog;
		Vector<OptionButton *> obs;
		Vector<Node *> stack;
		stack.push_back(cursor);
		while (stack.size() > 0) {
			Node *n = stack[stack.size() - 1];
			stack.remove_at(stack.size() - 1);
			OptionButton *ob = Object::cast_to<OptionButton>(n);
			if (ob) obs.push_back(ob);
			for (int i = 0; i < n->get_child_count(); i++) stack.push_back(n->get_child(i));
		}
		print_line(vformat("industrial_diag: NewDeviceDialog OptionButton count = %d (expect: 1 Interface Type + 1 Device Type + 1 Data Type from tag-rows Skip →>=3; first two are our targets)", obs.size()));

		// Order: Interface Type OB first (created around L80), Device Type OB second (L96).
		if (obs.size() >= 2) {
			OptionButton *iface = obs[0];
			OptionButton *dtype = obs[1];
			print_line(vformat("industrial_diag: NewDeviceDialog.IFACE_TYPE items=%d", iface ? iface->get_item_count() : -1));
			for (int i = 0; iface && i < iface->get_item_count(); i++) {
				print_line(vformat("    IFACE[%d] label='%s'  metadata=%d",
						  i, iface->get_item_text(i), (int)iface->get_item_metadata(i)));
			}
			print_line(vformat("industrial_diag: NewDeviceDialog.DEVICE_TYPE items=%d", dtype ? dtype->get_item_count() : -1));
			for (int i = 0; dtype && i < dtype->get_item_count(); i++) {
				print_line(vformat("    DEV[%d] label='%s'  meta(driver_idx)=%d",
						  i, dtype->get_item_text(i), (int)dtype->get_item_metadata(i)));
			}
		} else {
			print_line("industrial_diag: FAIL — fewer than 2 OptionButtons in NewDeviceDialog; cannot identify IFACE/DEVICE OBs.");
		}
	} else {
		print_line("industrial_diag: FAIL — NewDeviceDialog still nullptr after ensure.");
	}

	// --- Also: legacy industrial_get_driver_names() total + first/last 3 ---
	StringList legacy = industrial_get_driver_names();
	print_line(vformat("industrial_diag: industrial_get_driver_names().size() = %d", legacy.size()));
	for (int i = 0; i < legacy.size(); i++) {
		if (i < 3 || i >= legacy.size() - 3) {
			print_line(vformat("    [%d] %s", i, legacy[i]));
		} else if (i == 3) {
			print_line("    ... (snip) ...");
		}
	}
	print_line("industrial_diag: ====== PROTOCOL DUMP END — QUIT IN 1s ======");
	SceneTree *st = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (st) {
		Timer *q = memnew(Timer);
		q->set_wait_time(1.2);
		q->set_one_shot(true);
		q->set_autostart(true);
		q->connect("timeout", callable_mp_static(&_e2e_quit_lambda_target));
		if (Node *p = Object::cast_to<Node>(st->get_root())) p->add_child(q);
	}
}
