# Valgrind Bridge SDK

A generic Valgrind tool framework ("Bridge") that lets you write memory-analysis plugins without touching Valgrind internals. The SDK instruments every load/store in guest code and dispatches events to whichever plugin is linked in at build time.

This repository contains both the SDK and the reference **SharedState Tracker** plugin.

---

## Repository layout

```
bridge/            SDK — Valgrind tool core (VEX instrumentation + event dispatch)
include/           SDK — Public API headers (plugin interface, client macros, utilities)
  bridge_plugin.h    Plugin contract: implement bridge_plugin_t, call BRIDGE_PLUGIN_REGISTER()
  bridge_client.h    Target-side macros: BRIDGE_MONITOR_START / BRIDGE_MONITOR_STOP
  bridge_client.hpp  C++ RAII wrapper for bridge_client.h
  bridge_utils.h     Valgrind-safe helpers for plugin authors (alloc, strings, stack traces…)
plugins/           Plugins (each subdirectory is an independent plugin)
  sharedstate/       Reference plugin: detects cross-thread shared-state accesses
tests/             Integration tests for the sharedstate plugin
Makefile           Plugin-configurable build (set PLUGIN_DIR= to select a plugin)
.devcontainer/     VS Code dev container (Ubuntu 24.04 + valgrind + gcc)
```

### SDK / Plugin boundary

`bridge/` and `include/` form the **SDK**. They have zero knowledge of any specific plugin — all plugin logic is dispatched through function pointers in `bridge_plugin_t`. `plugins/` contains **consumers** of that SDK.

The boundary is enforced by design:
- `bridge/*.c` never `#include` anything from `plugins/`
- `include/*.h` expose only generic types and Valgrind `pub_tool_*` wrappers
- A plugin's only coupling to the SDK is implementing `bridge_plugin_t` and calling `BRIDGE_PLUGIN_REGISTER()`

When a second plugin or external consumer appears, splitting into separate repositories is straightforward: publish `bridge/` + `include/` as an SDK package and let each plugin repo depend on it.

---

## Requirements

- `gcc`, `make`
- `valgrind` and Valgrind development headers (`valgrind-dev` / `valgrind`)

```bash
# Debian / Ubuntu
sudo apt install valgrind gcc make
```

The `.devcontainer/` setup handles this automatically for VS Code users.

---

## Build and install

```bash
# Build the tool binary (default plugin: plugins/sharedstate)
make

# Install into Valgrind's libexec so you can use --tool=bridge
sudo make install

# Run your program under the bridge tool
valgrind --tool=bridge ./your_program
```

To build with a different plugin:
```bash
make PLUGIN_DIR=plugins/myplugin
```

---

## Running tests

```bash
make test
```

Builds the test programs in `tests/` and runs each one under `valgrind --tool=bridge`.

---

## Writing a new plugin

1. Create a directory under `plugins/` (or anywhere — path is configurable).
2. Implement `bridge_plugin_t` from `include/bridge_plugin.h`:

```c
#include "bridge_plugin.h"

static void on_mem_access(BgMemAccess* ma) { /* ... */ }
static void on_monitor_start(ThreadId tid, const HChar* label) { /* ... */ }
static void on_monitor_stop(ThreadId tid) { /* ... */ }

static const bridge_plugin_t my_plugin = {
    .name             = "myplugin",
    .on_mem_access    = on_mem_access,
    .on_monitor_start = on_monitor_start,
    .on_monitor_stop  = on_monitor_stop,
    /* other callbacks are optional — set to NULL to skip */
};

BRIDGE_PLUGIN_REGISTER(&my_plugin);
```

3. Build:

```bash
make PLUGIN_DIR=plugins/myplugin
sudo make install PLUGIN_DIR=plugins/myplugin
```

See `plugins/sharedstate/` for a complete worked example.

---

## Instrumenting your target program

Include `bridge_client.h` (or `bridge_client.hpp` for C++) and wrap the region of interest:

```c
#include "bridge_client.h"

BRIDGE_MONITOR_START("my-label");
/* ... code to analyse ... */
BRIDGE_MONITOR_STOP();
```

The macros are no-ops when the program runs outside Valgrind.
