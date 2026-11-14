.DEFAULT_GOAL := all

ARCH ?= aarch64
BUILD_ROOT ?= build
NX_WARNINGS_AS_ERRORS ?= 1
NX_STRICT ?= 1
HOST_OS ?= $(shell uname -s)

USERKIT_ROOT := $(CURDIR)
BUILD_DIR := $(abspath $(BUILD_ROOT))/$(ARCH)
OUT_DIR := $(BUILD_DIR)/userkit
OBJ_DIR := $(BUILD_DIR)/obj/userkit
SDK_ROOT ?= $(NEXS_SDK_ROOT)
ifeq ($(strip $(SDK_ROOT)),)
  $(error SDK_ROOT or NEXS_SDK_ROOT must name an NX-SDK source/install root)
endif
NXSDK_ROOT := $(abspath $(SDK_ROOT))
include $(NXSDK_ROOT)/config/extension-paths.mk
include $(NXSDK_ROOT)/config/userkit-paths.mk
include $(NXSDK_ROOT)/config/toolchain-versions.env
SDK_SOURCE_MODE := $(if $(wildcard $(NXSDK_ROOT)/lib/lib.c),1,0)

ifeq ($(HOST_OS),Darwin)
  AMD64_CROSS_COMPILE ?= x86_64-elf-
  AARCH64_CROSS_COMPILE ?= aarch64-none-elf-
else ifeq ($(HOST_OS),Linux)
  AMD64_CROSS_COMPILE ?= x86_64-elf-
  AARCH64_CROSS_COMPILE ?= aarch64-none-elf-
else
  $(error unsupported HOST_OS $(HOST_OS))
endif

ifeq ($(findstring amd64,$(ARCH)),amd64)
  override ARCH := amd64
  CROSS_COMPILE ?= $(AMD64_CROSS_COMPILE)
  ARCH_CFLAGS := -DARCH_AMD64 -mno-red-zone -mcmodel=large -Wa,--noexecstack
else ifeq ($(findstring aarch64,$(ARCH)),aarch64)
  override ARCH := aarch64
  CROSS_COMPILE ?= $(AARCH64_CROSS_COMPILE)
  ARCH_CFLAGS := -DARCH_AARCH64 -mcpu=cortex-a57
else
  $(error ARCH must be amd64 or aarch64)
endif

CC := $(CROSS_COMPILE)gcc
HOST_CC ?= cc
COMMON_FLAGS := -Wall -Wextra -Wpedantic -Wshadow -Wwrite-strings \
  -Wmissing-prototypes -Wstrict-prototypes -Wconversion -Wsign-conversion \
  -ffreestanding -fno-builtin -nostdlib -nostartfiles -fno-common \
  -fstack-protector-strong -fno-pic -fno-pie -fno-omit-frame-pointer -O2 -g
ifeq ($(NX_WARNINGS_AS_ERRORS),1)
COMMON_FLAGS += -Werror
endif
ifeq ($(NX_STRICT),1)
COMMON_FLAGS += -DNX_STRICT
endif
USERKIT_CFLAGS := $(COMMON_FLAGS) $(ARCH_CFLAGS) \
  -I$(NXSDK_ROOT)/include/contract -I$(NXSDK_ROOT)/include/api \
  $(NXSDK_EXTENSION_INCLUDE_FLAGS) $(NXSDK_USERKIT_INCLUDE_FLAGS) \
  -I$(USERKIT_ROOT)/tests/runtime -ffunction-sections -fdata-sections
USERKIT_LDFLAGS := -Wl,--gc-sections -Wl,-Ttext=0x80000000 -Wl,-e,_start
include catalog.mk
USERKIT_INTERNAL_INCLUDE_FLAGS := $(addprefix -I$(USERKIT_ROOT)/,$(NEXS_USERKIT_INTERNAL_INCLUDE_REL))
USERKIT_CFLAGS += $(USERKIT_INTERNAL_INCLUDE_FLAGS)
SERVICE_PROJECT_LIBS = $(foreach lib,$(NEXS_USERKIT_PROJECT_GLOBAL_LIBS_$(1)),$(USERKIT_GLOBAL_LIBRARY_$(lib)))
SERVICE_PROJECTS := $(NEXS_USERKIT_ALL_SERVICE_PROJECTS)
SBIN_PROJECTS := $(NEXS_USERKIT_SBIN_PROJECTS)
SERVICE_DIR = $(if $(NEXS_USERKIT_SERVICE_DIR_$(1)),$(NEXS_USERKIT_SERVICE_DIR_$(1)),services/$(1))
SERVICE_EXTRA_SRCS = $(NEXS_USERKIT_PROJECT_EXTRA_SOURCES_$(1))
SERVICE_EXTRA_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(call SERVICE_EXTRA_SRCS,$(1)))
SERVICE_ELFS := $(foreach project,$(SERVICE_PROJECTS),$(OUT_DIR)/services/$(project).elf)
RUNTIME_TEST_ELFS := $(foreach project,$(SERVICE_PROJECTS),$(OUT_DIR)/tests/test_$(project).elf)
SBIN_ELFS := $(foreach project,$(SBIN_PROJECTS),$(OUT_DIR)/sbin/$(project).elf)
SBIN_COMMON_OBJECT := $(OBJ_DIR)/sbin/common/nxinit_runtime.o

ifeq ($(SDK_SOURCE_MODE),1)
  SDK_BUILD_ROOT := $(abspath $(BUILD_ROOT))/sdk
  NXSDK_BUILD_DIR := $(SDK_BUILD_ROOT)/$(ARCH)
  NXSDK_OUTPUT_NAMESPACE := system
  include $(NXSDK_ROOT)/sdk.mk
  SDK_LINK_INPUTS := $(NXSDK_CORE_LIB_OBJECTS) $(NXSDK_MALLOC_OBJECT) $(NXSDK_SYSCALL_OBJECT)
  .PHONY: sdk
  sdk:
	@$(MAKE) -C "$(NXSDK_ROOT)" ARCH="$(ARCH)" BUILD_ROOT="$(SDK_BUILD_ROOT)" core
else
  include $(NXSDK_ROOT)/runtime/$(ARCH)/nxsdk-config.mk
  SDK_INSTALLED_ARCHIVE := $(firstword $(wildcard $(NEXS_SDK_LIBDIR)/libnxsdk-core.a $(NEXS_SDK_LIBDIR)/libnxsdk.a))
  SDK_LINK_INPUTS := $(SDK_INSTALLED_ARCHIVE) $(NEXS_SDK_CRTDIR)/syscall.o
  .PHONY: sdk
  sdk:
	@test -n "$(SDK_INSTALLED_ARCHIVE)" && test -s "$(SDK_INSTALLED_ARCHIVE)"
	@test -s "$(NEXS_SDK_CRTDIR)/syscall.o"
endif

.PHONY: all services sbin runtime-tests check check-prebuild check-power12 check-nxttyd-model require-global-libraries clean help
all: services sbin runtime-tests
require-global-libraries:
	@set -eu; $(foreach lib,$(NEXS_USERKIT_REQUIRED_GLOBAL_LIBS),value="$(USERKIT_GLOBAL_LIBRARY_$(lib))"; test -n "$$value" && test -s "$$value" || { echo "USERKIT missing global library mapping: $(lib)" >&2; exit 1; };)
services: sdk require-global-libraries $(SERVICE_ELFS)
sbin: sdk $(SBIN_ELFS)
runtime-tests: sdk $(RUNTIME_TEST_ELFS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p "$(dir $@)"
	@$(CC) $(USERKIT_CFLAGS) -MMD -MP -c "$<" -o "$@"

define SERVICE_template
$(OBJ_DIR)/services/$(1).o: $$(call SERVICE_DIR,$(1))/src/main.c
	@mkdir -p "$$(dir $$@)"
	@$$(CC) $$(USERKIT_CFLAGS) -MMD -MP -c "$$<" -o "$$@"

$(OUT_DIR)/services/$(1).elf: $(OBJ_DIR)/services/$(1).o $$(call SERVICE_EXTRA_OBJS,$(1)) $(SDK_LINK_INPUTS) $$(call SERVICE_PROJECT_LIBS,$(1))
	@mkdir -p "$$(dir $$@)"
	@$$(CC) $$(USERKIT_CFLAGS) $$(USERKIT_LDFLAGS) -o "$$@" 	  $(OBJ_DIR)/services/$(1).o $$(call SERVICE_EXTRA_OBJS,$(1)) $$(SDK_LINK_INPUTS) $$(call SERVICE_PROJECT_LIBS,$(1)) -lgcc

$(OBJ_DIR)/tests/$(1).o: $$(call SERVICE_DIR,$(1))/tests/runtime.c
	@mkdir -p "$$(dir $$@)"
	@$$(CC) $$(USERKIT_CFLAGS) -MMD -MP -c "$$<" -o "$$@"

$(OUT_DIR)/tests/test_$(1).elf: $(OBJ_DIR)/tests/$(1).o $(SDK_LINK_INPUTS)
	@mkdir -p "$$(dir $$@)"
	@$$(CC) $$(USERKIT_CFLAGS) $$(USERKIT_LDFLAGS) -o "$$@" "$$<" $$(SDK_LINK_INPUTS) -lgcc
endef
$(foreach service,$(SERVICE_PROJECTS),$(eval $(call SERVICE_template,$(service))))

$(SBIN_COMMON_OBJECT): sbin/common/nxinit_runtime.c
	@mkdir -p "$(dir $@)"
	@$(CC) $(USERKIT_CFLAGS) -MMD -MP -c "$<" -o "$@"

define SBIN_template
$(OBJ_DIR)/sbin/$(1).o: sbin/$(1)/src/main.c
	@mkdir -p "$$(dir $$@)"
	@$$(CC) $$(USERKIT_CFLAGS) -MMD -MP -c "$$<" -o "$$@"

$(OUT_DIR)/sbin/$(1).elf: $(OBJ_DIR)/sbin/$(1).o $(SBIN_COMMON_OBJECT) $(SDK_LINK_INPUTS)
	@mkdir -p "$$(dir $$@)"
	@$$(CC) $$(USERKIT_CFLAGS) $$(USERKIT_LDFLAGS) -o "$$@" \
	  $(OBJ_DIR)/sbin/$(1).o $(SBIN_COMMON_OBJECT) $$(SDK_LINK_INPUTS) -lgcc
endef
$(foreach program,$(SBIN_PROJECTS),$(eval $(call SBIN_template,$(program))))

check-prebuild:
	@python3 "$(NXSDK_ROOT)/tools/userkit/check_catalog.py" \
	  --project-root "$(USERKIT_ROOT)" --sdk-root "$(NXSDK_ROOT)"
	@set -eu; find sbin services -type f -path '*/tests/project_test.py' -print | sort | \
	  while IFS= read -r test; do python3 "$$test"; done

check-nxttyd-model:
	@mkdir -p "$(BUILD_DIR)/host-tests"
	@$(HOST_CC) -std=c11 -Wall -Wextra -Werror \
	  $(NXSDK_USERKIT_INCLUDE_FLAGS) -I$(NXSDK_ROOT)/include/contract \
	  services/nxttyd/lib/tty_model.c services/nxttyd/tests/tty_model_test.c \
	  -o "$(BUILD_DIR)/host-tests/nxttyd_model_test"
	@"$(BUILD_DIR)/host-tests/nxttyd_model_test"

check-power12:
	@python3 "$(NXSDK_ROOT)/tools/userkit/power12/run.py" \
	  --project-root "$(USERKIT_ROOT)" \
	  --policy "$(USERKIT_ROOT)/power12-policy.json" \
	  --sdk-root "$(NXSDK_ROOT)" \
	  --report-dir "$(BUILD_DIR)/reports/power12"

check: check-prebuild check-power12 check-nxttyd-model all
	@echo "NX-USERKIT $(ARCH) check passed"

DEPS := $(shell find "$(OBJ_DIR)" -type f -name '*.d' 2>/dev/null)
-include $(DEPS)

clean:
	@rm -rf "$(BUILD_ROOT)"

help:
	@echo "NX-USERKIT targets: all services sbin runtime-tests check check-prebuild check-power12"
	@echo "Variables: ARCH=amd64|aarch64 BUILD_ROOT=... SDK_ROOT=..."
