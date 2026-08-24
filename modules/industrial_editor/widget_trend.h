#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class WidgetTrend : public RefCounted {
	GDCLASS(WidgetTrend, RefCounted);

protected:
	static void _bind_methods();

public:
	String get_version() const;
};
