#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class WidgetMeter : public RefCounted {
	GDCLASS(WidgetMeter, RefCounted);

protected:
	static void _bind_methods();

public:
	String get_version() const;
};
