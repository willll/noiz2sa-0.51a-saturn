# smpc-SRL
Basic functions for Sega Saturn SMPC commands

## Use
Copy `modules_extra` to your SRL folder

To include a module in your project, add this line to your project’s Makefile:
```
MODULES_EXTRA = smpc
```
Include it in your code:
```
#include <smpc.hpp>
```
Currently only for turning the audio processor on/off and enabling/disabling the reset button are enabled.

These are for use with the Ponesound-SRL module and a coming Backup module.



