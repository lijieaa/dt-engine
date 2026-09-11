#pragma once

#include "keypad_backend.h"

class TscnKeypadBackend : public KeypadBackend {
	GDCLASS(TscnKeypadBackend, KeypadBackend);

protected:
	static void _bind_methods();

public:
	bool can_open(const KeypadDefinition &p_definition) const override;
	KeypadView *open(const KeypadOpenRequest &p_request, const KeypadDefinition &p_definition) override;
	void close(KeypadView *p_instance) override;
};
