# NX-USERKIT

NX-USERKIT owns the privileged **user-mode system-service implementations** of NexsOS. It does not own the kernel, ordinary system applications, or external libraries.

Each service is an independent project below `services/`, with its own `project.mk`, source, pre-build contract test and runtime supervision test. `init/nxinit` is a separate authenticated manifest-loader project. The public wire/client contracts are owned by NX-SDK under `include/api` and `lib/extensions/userkit`; USERKIT consumes them and may replace any service implementation that preserves the same contract.

Canonical projects are `nxntfy_srv`, `nxexecd`, `nxtimerdd`, `nxpaxd`, `nxcompositor`, `nxkeyboardd`, `nxmoused`, `nxttyd`, plus the non-persistent `nxinit` initializer.

`make check ARCH=...` runs catalog/pre-build checks, the strict USERKIT Power12 gate and compiles every service and every runtime probe. Runtime probes are installed by the superproject and validate the manager's authoritative `system.manage.service.<name>` generation record after boot.

## Current modular layout

`init/` has been replaced by the general `sbin/` project catalog. The current sbin projects are `nxinit` and `nxdemo1init`; both use the same authenticated NXMANAGE protocol through a shared runtime and expose their public identity/runtime headers from NX-SDK. `services/example/` contains alternate service implementations used by `demo1`.

Build/audit tooling is SDK-owned under `system/tools/userkit`; NX-USERKIT retains only its project policy and project-owned tests. `make check ARCH=<arch> SDK_ROOT=<sdk>` is the authoritative standalone gate.
