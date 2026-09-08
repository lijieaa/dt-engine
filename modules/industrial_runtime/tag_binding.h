#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "scene/main/node.h"

class IndustrialRuntime;

// TagBinding connects one or more industrial tags (via IndustrialRuntime) to a
// single property on the binding's parent node. Example setup in the editor:
//
//   Label (text: "T1=—")
//     └── TagBinding
//           tags = ["T1"]                 # $T1 used in expression
//           expression = "$T1"             # simple identity expression
//           target_property = "text"      # write result to Label.text
//           enable_write = false          (default)
//
// TagBinding is PRD §19's LinkBinding on the client side: whenever any of
// its tags changes, it re-evaluates the expression and writes the result to
// the parent. The heavy lifting (evaluator, cache, protocol) lives in the
// IndustrialRuntime module so this class stays only about wiring.
class TagBinding : public Node {
	GDCLASS(TagBinding, Node);

public:
	TagBinding() = default;
	~TagBinding() override;

	// Exposed to the editor inspector.
	void set_tags(const Array &p_tags);
	Array get_tags() const;

	void set_expression(const String &p_expr);
	String get_expression() const;

	void set_target_property(const StringName &p_name);
	StringName get_target_property() const;

	void set_fallback(const Variant &v);
	Variant get_fallback() const;

	void set_enable_write(bool enable);
	bool get_enable_write() const;

	// When enable_write is true AND `write_source_signal` names a signal on
	// the parent (e.g. "pressed" on a Button), the binding will connect it
	// and invoke Runtime.write_tag on the first configured tag with value
	// derived from the `write_source_property` on the parent (e.g. "value"
	// on a SpinBox, "pressed" bool on a CheckButton).
	void set_write_source_signal(const String &sig);
	String get_write_source_signal() const;

	void set_write_source_property(const StringName &p_name);
	StringName get_write_source_property() const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

private:
	Array tags;
	String expression;
	StringName target_property = SNAME("text");
	Variant fallback;
	bool enable_write = false;
	String write_source_signal;
	StringName write_source_property;
	String write_tag;       // cached from tags[0]
	bool write_conn = false;

	void _bind_runtime_signals();
	void _apply_now(const Dictionary &snapshot);
	void _on_runtime_tag_changed(const String &tag, const Variant &value,
			const String &quality, int64_t version, int64_t ts_ms);
	void _do_write();
	static IndustrialRuntime *_runtime_or_null();
};
