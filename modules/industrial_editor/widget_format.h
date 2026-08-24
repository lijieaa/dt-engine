#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class WidgetFormat : public RefCounted {
	GDCLASS(WidgetFormat, RefCounted);

protected:
	static void _bind_methods();

public:
	String get_version() const;
};
