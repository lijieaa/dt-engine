#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

// ExprEval is a tiny recursive-descent expression evaluator designed
// specifically for TagBinding expressions. It supports:
//
//   Literals:      42   3.14   "x"   true   false   null
//   Identifiers:   $TagName          — value of the named tag (Variant)
//                  quality("tag")    — quality string ("good"/"bad"/...)
//                  is_good("tag")    — true iff quality == "good"
//                  min(a,b) max(a,b) clamp(x,lo,hi) round(x) abs(x)
//   Arithmetic:    +  -  *  /  %  unary -
//   Comparison:    ==  !=  <  <=  >  >=  (return bool)
//   Logical:       &&  ||  !          (short-circuit)
//   Parens:        (...)
//
// The evaluator never throws; errors are stored in `has_error`/`error_message`
// and an empty Variant is returned. This keeps bindings from crashing the UI
// process when an expression references a tag that has not arrived yet.
class ExprEval : public RefCounted {
	GDCLASS(ExprEval, RefCounted);

protected:
	static void _bind_methods();

public:
	// C++ entry point. `tag_snapshot` is a Dictionary mapping tag-name → flat
	// Dictionary (same shape as TagCache::get).
	Variant evaluate(const String &expression, const Dictionary &tag_snapshot, String &r_error_message);

	// GDScript convenience: returns a Dictionary { "ok": bool, "value": Variant, "error": String }.
	Dictionary eval(const String &expression, const Dictionary &tag_snapshot);

	// Syntax-only helper, returns a best-effort list of $TagName references so
	// the binding system knows which tags to subscribe to even before values
	// arrive.
	Array extract_tag_refs(const String &expression);

private:
	struct Parser {
		const String &src;
		int pos = 0;
		String error;
		const Dictionary &snapshot;

		Parser(const String &s, const Dictionary &sn) : src(s), snapshot(sn) {}

		char32_t peek() const { return pos < src.length() ? src[pos] : U'\0'; }
		char32_t bump() { return pos < src.length() ? src[pos++] : U'\0'; }
		void skip_ws();
		bool accept(char32_t c);
		bool expect(char32_t c, const char *what);

		Variant parse_or();
		Variant parse_and();
		Variant parse_eq();
		Variant parse_rel();
		Variant parse_add();
		Variant parse_mul();
		Variant parse_unary();
		Variant parse_primary();

		Variant parse_tag_ref();   // starts with '$'
		Variant parse_fun_call();  // ident( ... ) — stdlib quality/is_good/...
		Variant parse_string();    // double-quoted strings, no escapes
		Variant parse_number();    // integer or double, optional - sign handled by unary
		Variant parse_ident();     // bare identifier (for built-in constants + funcs)
	};

	static double to_double(const Variant &v, bool &ok);
	static bool to_bool(const Variant &v);
	static bool compare(const Variant &a, const Variant &b, int op, bool &ok);
};
