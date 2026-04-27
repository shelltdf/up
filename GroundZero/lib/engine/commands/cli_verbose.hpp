#pragma once

namespace gz {

void set_cli_verbose(bool on);
bool cli_verbose();
void cli_verbose_phase(const char* command, const char* phase);

}  // namespace gz
