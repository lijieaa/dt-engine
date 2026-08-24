#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class WidgetAlarm : public RefCounted {
	GDCLASS(WidgetAlarm, RefCounted);

protected:
	static void _bind_methods();

public:
	String get_version() const;
};
