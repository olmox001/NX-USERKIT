# Canonical NX-USERKIT project catalog.
NEXS_USERKIT_SERVICE_PROJECTS := \
  nxntfy_srv nxexecd nxtimerdd nxpaxd nxcompositor nxkeyboardd nxmoused nxttyd
NEXS_USERKIT_EXAMPLE_PROJECTS := nxnotify_registry nxexample_echo
NEXS_USERKIT_ALL_SERVICE_PROJECTS := $(NEXS_USERKIT_SERVICE_PROJECTS) $(NEXS_USERKIT_EXAMPLE_PROJECTS)
NEXS_USERKIT_SBIN_PROJECTS := nxinit nxdemo1init

NEXS_USERKIT_SERVICE_DIR_nxnotify_registry := services/example/nxnotify_registry
NEXS_USERKIT_SERVICE_DIR_nxexample_echo := services/example/nxexample_echo

# Project-local implementation pieces: never exported by NX-SDK.
NEXS_USERKIT_INTERNAL_INCLUDE_REL := services/nxttyd/include
NEXS_USERKIT_PROJECT_INCLUDE_REL_nxttyd := services/nxttyd/include
NEXS_USERKIT_PROJECT_EXTRA_SOURCES_nxttyd := services/nxttyd/lib/tty_model.c

# Global-library requirements are symbolic. The integrating user project maps
# each name to an archive; USERKIT never knows its repository or vendor path.
NEXS_USERKIT_REQUIRED_GLOBAL_LIBS := keyboard_layout
NEXS_USERKIT_PROJECT_GLOBAL_LIBS_nxkeyboardd := keyboard_layout
