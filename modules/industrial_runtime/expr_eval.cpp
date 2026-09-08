#include "expr_eval.h"
#include "value_convert.h"

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/print_string.h"
#include "core/variant/array.h"

void ExprEval::_bind_methods() {
	ClassDB::bind_method(D_METHOD("eval", "expression", "tag_snapshot"), &ExprEval::eval);
	ClassDB::bind_method(D_METHOD("extract_tag_refs", "expression"), &ExprEval::extract_tag_refs);
}

Variant ExprEval::evaluate(const String &expression, const Dictionary &snapshot, String &r_err) {
	Parser p(expression, snapshot);
	Variant result = p.parse_or();
	if (p.error.length() > 0) {
		r_err = p.error;
		return Variant();
	}
	// Any trailing junk (except whitespace) counts as parse error.
	p.skip_ws();
	if (p.pos < p.src.length()) {
		r_err = String("unexpected trailing input at position ") + itos(p.pos) + String(": '") + String(&p.src.ptr()[p.pos], MIN(20, p.src.length() - p.pos)) + "'";
		return Variant();
	}
	r_err = String();
	return result;
}

Dictionary ExprEval::eval(const String &expression, const Dictionary &tag_snapshot) {
	Dictionary out;
	String err;
	Variant v = evaluate(expression, tag_snapshot, err);
	if (err.length() > 0) {
		out["ok"] = false;
		out["error"] = err;
		out["value"] = Variant();
	} else {
		out["ok"] = true;
		out["error"] = String();
		out["value"] = v;
	}
	return out;
}

Array ExprEval::extract_tag_refs(const String &expression) {
	Array out;
	HashMap<String, bool> seen;
	int i = 0;
	while (i < expression.length()) {
		char32_t c = expression[i];
		if (c == '$' && i + 1 < expression.length()) {
			i++;
			int start = i;
			while (i < expression.length()) {
				char32_t k = expression[i];
				bool cont = (k == '_') || (k == '.') || (k == '/') ||
							(k >= 'a' && k <= 'z') || (k >= 'A' && k <= 'Z') ||
							(k >= '0' && k <= '9');
				if (!cont) break;
				i++;
			}
			String name(&expression.ptr()[start], i - start);
			if (name.length() > 0 && !seen.has(name)) {
				seen.insert(name, true);
				out.push_back(name);
			}
			continue;
		}
		i++;
	}
	return out;
}

// ---------- Parser utilities -----------------------------------------

void ExprEval::Parser::skip_ws() {
	while (pos < src.length()) {
		char32_t c = src[pos];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { pos++; continue; }
		break;
	}
}

bool ExprEval::Parser::accept(char32_t c) {
	skip_ws();
	if (peek() == c) { pos++; return true; }
	return false;
}

bool ExprEval::Parser::expect(char32_t c, const char *what) {
	skip_ws();
	if (peek() == c) { pos++; return true; }
	if (error.length() == 0) {
		error = String("expected ") + String(what) + String(" at ") + itos(pos);
	}
	return false;
}

double ExprEval::to_double(const Variant &v, bool &ok) {
	ok = true;
	switch (v.get_type()) {
		case Variant::INT: return (double)int64_t(v);
		case Variant::FLOAT: return (double)v;
		case Variant::BOOL: return bool(v) ? 1.0 : 0.0;
		case Variant::STRING: {
			double d = 0;
			if (String(v).to_double(&d)) return d;
			ok = false; return 0;
		}
		default:
			ok = false;
			return 0;
	}
}

bool ExprEval::to_bool(const Variant &v) {
	switch (v.get_type()) {
		case Variant::BOOL: return bool(v);
		case Variant::INT: return int64_t(v) != 0;
		case Variant::FLOAT: return (double)v != 0.0;
		case Variant::STRING: return String(v).length() > 0;
		default: return false;
	}
}

bool ExprEval::compare(const Variant &a, const Variant &b, int op, bool &ok) {
	ok = true;
	bool n_ok;
	double ad = to_double(a, n_ok);
	if (n_ok) {
		double bd = to_double(b, n_ok);
		if (n_ok) {
			switch (op) {
				case 0: return ad == bd;
				case 1: return ad != bd;
				case 2: return ad <  bd;
				case 3: return ad <= bd;
				case 4: return ad >  bd;
				case 5: return ad >= bd;
			}
		}
	}
	// Fallback: use Variant::operator== / string compare for != / ==.
	switch (op) {
		case 0: return a == b;
		case 1: return !(a == b);
		default:
			ok = false;
			return false;
	}
}

// ---------- Precedence ladder ----------------------------------------

Variant ExprEval::Parser::parse_or() {
	Variant a = parse_and();
	while (error.length() == 0 && accept('|')) {
		if (!accept('|')) {
			if (error.length() == 0) error = String("expected '||' at ") + itos(pos);
			return Variant();
		}
		Variant b = parse_and();
		a = to_bool(a) || to_bool(b);
	}
	return a;
}

Variant ExprEval::Parser::parse_and() {
	Variant a = parse_eq();
	while (error.length() == 0 && accept('&')) {
		if (!accept('&')) {
			if (error.length() == 0) error = String("expected '&&' at ") + itos(pos);
			return Variant();
		}
		Variant b = parse_eq();
		a = to_bool(a) && to_bool(b);
	}
	return a;
}

Variant ExprEval::Parser::parse_eq() {
	Variant a = parse_rel();
	while (error.length() == 0) {
		skip_ws();
		int op = -1;
		if (accept('=')) { expect('=', "="); op = 0; }
		else if (accept('!')) { if (expect('=', "=")) op = 1; else return Variant(); }
		else break;
		Variant b = parse_rel();
		bool ok = true;
		a = compare(a, b, op, ok);
		if (!ok && error.length() == 0) {
			error = String("comparison types unsupported at ") + itos(pos);
			return Variant();
		}
	}
	return a;
}

Variant ExprEval::Parser::parse_rel() {
	Variant a = parse_add();
	while (error.length() == 0) {
		skip_ws();
		int op = -1;
		char32_t c = peek();
		if (c == '<') { bump(); if (accept('=')) op = 3; else op = 2; }
		else if (c == '>') { bump(); if (accept('=')) op = 5; else op = 4; }
		else break;
		Variant b = parse_add();
		bool ok = true;
		a = compare(a, b, op, ok);
		if (!ok && error.length() == 0) {
			error = String("comparison failed at ") + itos(pos);
			return Variant();
		}
	}
	return a;
}

Variant ExprEval::Parser::parse_add() {
	Variant a = parse_mul();
	while (error.length() == 0) {
		skip_ws();
		char32_t c = peek();
		if (c != '+' && c != '-') break;
		bump();
		Variant b = parse_mul();
		bool oka, okb;
		double ad = to_double(a, oka);
		double bd = to_double(b, okb);
		if (oka && okb) {
			a = (c == '+') ? (ad + bd) : (ad - bd);
		} else if (c == '+' && (a.get_type() == Variant::STRING || b.get_type() == Variant::STRING)) {
			a = String(a) + String(b);
		} else if (error.length() == 0) {
			error = String("arithmetic requires numeric operands (found ") + Variant::get_type_name(a.get_type()) + " + " + Variant::get_type_name(b.get_type()) + ")";
			return Variant();
		}
	}
	return a;
}

Variant ExprEval::Parser::parse_mul() {
	Variant a = parse_unary();
	while (error.length() == 0) {
		skip_ws();
		char32_t c = peek();
		if (c != '*' && c != '/' && c != '%') break;
		bump();
		Variant b = parse_unary();
		bool oka, okb;
		double ad = to_double(a, oka);
		double bd = to_double(b, okb);
		if (!oka || !okb) {
			if (error.length() == 0) {
				error = String("*/% require numeric operands");
				return Variant();
			}
		}
		switch (c) {
			case '*': a = ad * bd; break;
			case '/':
				if (bd == 0.0) { if (error.length() == 0) { error = "division by zero"; return Variant(); } }
				a = ad / bd; break;
			case '%':
				if (bd == 0.0) { if (error.length() == 0) { error = "mod by zero"; return Variant(); } }
				a = Math::fmod(ad, bd); break;
		}
	}
	return a;
}

Variant ExprEval::Parser::parse_unary() {
	skip_ws();
	if (accept('-')) {
		Variant v = parse_unary();
		bool ok;
		double d = to_double(v, ok);
		if (!ok) { if (error.length() == 0) error = "unary minus expects numeric"; return Variant(); }
		return -d;
	}
	if (accept('!')) {
		Variant v = parse_unary();
		return !to_bool(v);
	}
	return parse_primary();
}

Variant ExprEval::Parser::parse_primary() {
	skip_ws();
	if (accept('(')) {
		Variant v = parse_or();
		if (!expect(')', ")")) return Variant();
		return v;
	}
	char32_t c = peek();
	if (c == '$') return parse_tag_ref();
	if (c == '"') return parse_string();
	if (c >= '0' && c <= '9') return parse_number();
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') return parse_ident();
	if (error.length() == 0) {
		error = String("unexpected character '") + String(&c, 1) + String("' at position ") + itos(pos);
	}
	return Variant();
}

Variant ExprEval::Parser::parse_tag_ref() {
	bump(); // consume '$'
	int start = pos;
	while (pos < src.length()) {
		char32_t k = src[pos];
		bool cont = (k == '_') || (k == '.') || (k == '/') ||
					(k >= 'a' && k <= 'z') || (k >= 'A' && k <= 'Z') ||
					(k >= '0' && k <= '9');
		if (!cont) break;
		pos++;
	}
	String name(&src.ptr()[start], pos - start);
	if (name.length() == 0) {
		if (error.length() == 0) error = String("empty tag name after '$' at ") + itos(start);
		return Variant();
	}
	if (!snapshot.has(name)) {
		// Tag not yet in snapshot (hasn't arrived). Treat as nil so caller can
		// distinguish this from legitimate 0/false. Call sites keep displaying
		// the last-known value in that case.
		return Variant();
	}
	Dictionary flat = snapshot[name];
	return flat.get("value", Variant());
}

Variant ExprEval::Parser::parse_string() {
	bump(); // consume opening quote
	int start = pos;
	String out;
	while (pos < src.length()) {
		char32_t c = src[pos++];
		if (c == '"') return out;
		if (c == '\\' && pos < src.length()) {
			char32_t n = src[pos++];
			switch (n) {
				case 'n': out += '\n'; break;
				case 't': out += '\t'; break;
				case 'r': out += '\r'; break;
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				default: out += n; break;
			}
		} else {
			out += c;
		}
	}
	if (error.length() == 0) error = String("unterminated string starting at ") + itos(start);
	return Variant();
}

Variant ExprEval::Parser::parse_number() {
	int start = pos;
	bool has_dot = false;
	while (pos < src.length()) {
		char32_t c = src[pos];
		if (c >= '0' && c <= '9') { pos++; continue; }
		if (c == '.' && !has_dot) { has_dot = true; pos++; continue; }
		if ((c == 'e' || c == 'E') && pos + 1 < src.length()) {
			// Simple exponent support: advance past 'e' and optional sign/digits.
			pos++;
			if (src[pos] == '+' || src[pos] == '-') pos++;
			while (pos < src.length() && src[pos] >= '0' && src[pos] <= '9') pos++;
			has_dot = true; // Treat scientific notation as double.
			break;
		}
		break;
	}
	String text(&src.ptr()[start], pos - start);
	if (has_dot) {
		bool ok; double d = text.to_double(&ok);
		if (!ok && error.length() == 0) { error = String("bad number: ") + text; return Variant(); }
		return d;
	}
	bool ok; int64_t i = text.to_int(&ok);
	if (!ok && error.length() == 0) { error = String("bad int: ") + text; return Variant(); }
	return i;
}

Variant ExprEval::Parser::parse_ident() {
	int start = pos;
	while (pos < src.length()) {
		char32_t c = src[pos];
		bool cont = (c == '_') ||
					(c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
					(c >= '0' && c <= '9');
		if (!cont) break;
		pos++;
	}
	String name(&src.ptr()[start], pos - start);
	if (name == "true") return true;
	if (name == "false") return false;
	if (name == "null" || name == "nil" || name == "None") return Variant();

	if (accept('(')) {
		// Built-in function dispatch.
		Vector<Variant> args;
		if (!accept(')')) {
			while (true) {
				args.push_back(parse_or());
				skip_ws();
				if (accept(',')) continue;
				if (!expect(')', ")")) return Variant();
				break;
			}
		}
		#define IRT_ARG(n) (args.size() > n ? args[n] : Variant())
		#define IRT_NUM(n, out) { bool ok_; (out) = to_double(IRT_ARG(n), ok_); if (!ok_) { if (error.length() == 0) error = String(name + " expects numeric arg #" + itos(n+1)); return Variant(); } }
		if (name == "quality" && args.size() >= 1) {
			String tag = IRT_ARG(0);
			Dictionary flat = snapshot.get(tag, Dictionary());
			return flat.get("quality", String(ir_proto::QUALITY_STALE));
		}
		if (name == "is_good" && args.size() >= 1) {
			String tag = IRT_ARG(0);
			Dictionary flat = snapshot.get(tag, Dictionary());
			return String(flat.get("quality", String())) == String(ir_proto::QUALITY_GOOD);
		}
		if (name == "min" && args.size() >= 2) {
			double a, b; IRT_NUM(0, a); IRT_NUM(1, b); return MIN(a, b);
		}
		if (name == "max" && args.size() >= 2) {
			double a, b; IRT_NUM(0, a); IRT_NUM(1, b); return MAX(a, b);
		}
		if (name == "clamp" && args.size() >= 3) {
			double x, lo, hi; IRT_NUM(0, x); IRT_NUM(1, lo); IRT_NUM(2, hi); return CLAMP(x, lo, hi);
		}
		if (name == "round" && args.size() >= 1) {
			double x; IRT_NUM(0, x); return int64_t(Math::round(x));
		}
		if (name == "abs" && args.size() >= 1) {
			double x; IRT_NUM(0, x); return Math::abs(x);
		}
		if (name == "sqrt" && args.size() >= 1) {
			double x; IRT_NUM(0, x); return Math::sqrt(x);
		}
		if (name == "len" && args.size() >= 1) {
			Variant v = IRT_ARG(0);
			if (v.get_type() == Variant::STRING) return int64_t(String(v).length());
			if (v.get_type() == Variant::ARRAY) return int64_t(Array(v).size());
			if (error.length() == 0) { error = "len expects string or array"; return Variant(); }
		}
		if (error.length() == 0) error = String("unknown function: ") + name;
		return Variant();
	}
	if (error.length() == 0) error = String("unknown identifier: ") + name;
	return Variant();
}
