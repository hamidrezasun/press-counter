// pc_shell.h - text console on the USB port (CH340G, 115200 baud): status, settings, commands.  Type "help".
#ifndef PC_SHELL_H
#define PC_SHELL_H

namespace shell {
void begin();
void poll();
}  // namespace shell

#endif
