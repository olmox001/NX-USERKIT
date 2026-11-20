/*
 * nxinit — declarative system startup policy in userland.
 *
 * The machine boot provider publishes the selected init image identity and
 * nxmanageDD starts this process as ROOT. Init reads a protected manifest; it
 * declares order, one-shot dependencies and persistent locks over the
 * authenticated bounded manager port. It NEVER owns the restart queue or
 * process-reaping policy. The manager retains real process ownership.
 */
#include <nxmanage.h>
#include <nxinit_runtime.h>
#include <os1.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { NXINIT_MANIFEST_BYTES = 2048, NXINIT_LINE_BYTES = 160,
       NXINIT_ACK_POLL_MS = 50, NXINIT_PORT_RETRIES = 40 };
/* Manifest is owned by the single boot transaction, not global state. */

static int nxinit_boot_profile(void) {
  char mode[8] = {0};
  int n = file_read(NXMANAGE_PROFILE_CONFIG, mode, sizeof(mode) - 1U, 0);
  if (n != 2 || mode[1] != '\n' || (mode[0] != '0' && mode[0] != '1')) {
    OS1_report_error("nxinit.boot_profile", -EIO);
    return -1;
  }
  return mode[0] == '1' ? 1 : 0;
}

static int nxinit_port_claim(void) {
  /* Allocate an ordinary PROCESS READ handle, not a machine-control right.
   * This allocates the target handle table for the future kernel grant. */
  int root = (int)OS1low_handle_create(OS1_NS_PROC, "1",
                                      OS1_RIGHT_READ, OBJ_TYPE_PROCESS);
  if (root < 0)
    return root;
  if (OS1low_handle_close(root) != 0)
    return -EIO;
  struct ipc_message hello = {0};
  hello.type = NXMANAGE_REQ_HELLO;
  hello.data1 = NXMANAGE_PROTO_VERSION;
  if (OS1low_ipc_send(1, &hello) != 0)
    return -EIO;
  for (unsigned attempt = 0U; attempt < NXMANAGE_ACK_STEPS; ++attempt) {
    struct ipc_message reply = {0};
    if (OS1low_ipc_try_recv(-1, &reply) == 0) {
      if (reply.from != 1 || reply.type != NXMANAGE_REP_HELLO ||
          reply.data2 != NXMANAGE_PROTO_VERSION)
        continue;
      int handle = (int)reply.data1;
      long info = OS1low_cap_query(handle);
      if (handle < 0 || info < 0 ||
          OS1_CAPQ_TYPE(info) != OBJ_TYPE_PORT ||
          OS1_CAPQ_RIGHTS(info) != OS1_RIGHT_WRITE)
        return -EPERM;
      return handle;
    }
    OS1_sleep(NXINIT_ACK_POLL_MS);
  }
  return -EIO;
}

static int nxinit_pack(char *dst, unsigned cap, const char *path,
                       const char *args, const char *registry) {
  const char *fields[3] = {path, args, registry};
  unsigned at = 0U;
  for (unsigned i = 0U; i < 3U; ++i) {
    unsigned length = 0U;
    for (; length < 64U && fields[i][length] != '\0'; ++length) {
    }
    if (length >= cap || length + 1U > cap - at)
      return -1;
    for (unsigned k = 0U; k < 64U; ++k) {
      if (k <= length)
        dst[at + k] = fields[i][k];
    }
    at += length + 1U;
  }
  return 0;
}

static int nxinit_ack(unsigned slot, uint32_t expected_type) {
  for (unsigned poll = 0U; poll < NXMANAGE_ACK_STEPS; ++poll) {
    struct ipc_message reply = {0};
    if (OS1low_ipc_try_recv(-1, &reply) == 0) {
      if (reply.from != 1 || reply.type != (int)expected_type ||
          reply.data1 != slot) {
        OS1_report_error("nxinit.management_reply", -EPERM);
        continue;
      }
      return (int)(int64_t)reply.data2;
    }
    OS1_sleep(NXINIT_ACK_POLL_MS);
  }
  OS1_report_error("nxinit.ack_timeout", -EIO);
  return -EIO;
}

/* Service names are explicit immutable manifest metadata, transported over
 * the same authenticated bounded PORT as the registration body. Separate
 * NAME/REGISTER records avoid truncating the path or allocating another pipe. */
static int nxinit_name(int port, unsigned slot, const char *name) {
  struct ipc_message request = {0};
  unsigned len = 0U;
  if (name == NULL)
    return -EINVAL;
  for (; len < NXMANAGE_NAME_MAX && name[len] != '\0'; ++len) {
  }
  if (len == 0U || len >= NXMANAGE_NAME_MAX)
    return -EINVAL;
  request.type = NXMANAGE_REQ_NAME;
  request.data1 = slot;
  request.data2 = NXMANAGE_PROTO_VERSION;
  for (unsigned i = 0U; i < NXMANAGE_NAME_MAX; ++i) {
    if (i <= len)
      request.payload[i] = name[i];
  }
  if (OS1_port_send(port, &request) != (long)sizeof(request))
    return -EIO;
  return nxinit_ack(slot, NXMANAGE_REP_NAME);
}

/* Symbolic service rights come from the image's immutable policy manifest.
 * Unknown, repeated or truncated tokens are rejected, not interpreted as
 * broad ROOT authority. This is startup policy, never a kernel identity. */
static int nxinit_cap_token(const char *name, unsigned length,
                            uint32_t *bit) {
  if (length == 5U && strncmp(name, "spawn", length) == 0)
    *bit = CAP_SPAWN;
  else if (length == 8U && strncmp(name, "fs-write", length) == 0)
    *bit = CAP_FS_WRITE;
  else if (length == 7U && strncmp(name, "ipc-any", length) == 0)
    *bit = CAP_IPC_ANY;
  else if (length == 6U && strncmp(name, "window", length) == 0)
    *bit = CAP_WINDOW;
  else if (length == 14U &&
           strncmp(name, "registry-write", length) == 0)
    *bit = CAP_REG_WRITE;
  else if (length == 13U &&
           strncmp(name, "window-manage", length) == 0)
    *bit = CAP_WINDOW_MANAGE;
  else if (length == 12U && strncmp(name, "keyboard-raw", length) == 0)
    *bit = CAP_KEYBOARD_RAW;
  else if (length == 11U && strncmp(name, "pointer-raw", length) == 0)
    *bit = CAP_POINTER_RAW;
  else
    return -EINVAL;
  return 0;
}

static int nxinit_caps_from_manifest(const char *authority,
                                     uint32_t *caps) {
  if (authority == NULL || caps == NULL ||
      strncmp(authority, "caps:", 5U) != 0)
    return -EINVAL;
  uint32_t mask = 0U;
  unsigned start = 5U, seen = 0U;
  for (unsigned pos = 5U; pos < 64U; ++pos) {
    char ch = authority[pos];
    if (ch == ',' || ch == '\0') {
      unsigned len = pos - start;
      if (len == 0U) return -EINVAL;
      if (len == 4U && strncmp(authority + start, "none", len) == 0) {
        if (seen != 0U || ch != '\0') return -EINVAL;
        *caps = 0U;
        return 0;
      }
      uint32_t bit = 0U;
      if (nxinit_cap_token(authority + start, len, &bit) != 0 ||
          (mask & bit) != 0U)
        return -EINVAL;
      mask |= bit;
      ++seen;
      if (ch == '\0') {
        *caps = mask;
        return 0;
      }
      start = pos + 1U;
    }
  }
  return -EINVAL;
}

/* The selected image must declare every service's capability mask. A
 * missing field never falls back to CAP_ALL or an ambient ROOT preset. */
static int nxinit_auth(int port, unsigned slot, const char *authority) {
  uint32_t caps = 0U;
  int parsed = nxinit_caps_from_manifest(authority, &caps);
  if (parsed != 0) return parsed;
  struct ipc_message request = {0};
  request.type = NXMANAGE_REQ_AUTH;
  request.data1 = slot;
  request.data2 = caps;
  if (OS1_port_send(port, &request) != (long)sizeof(request))
    return -EIO;
  return nxinit_ack(slot, NXMANAGE_REP_AUTH);
}

static int nxinit_declare(int port, unsigned slot, uint32_t mode,
                          const char *name, const char *path,
                          const char *args, const char *registry,
                          const char *authority) {
  if (nxinit_name(port, slot, name) != 0)
    return -EPERM;
  int allowed = nxinit_auth(port, slot, authority);
  if (allowed != 0) return allowed;
  struct ipc_message message = {0};
  message.type = NXMANAGE_REQ_REGISTER;
  message.data1 = slot;
  message.data2 = mode;
  if (nxinit_pack(message.payload, sizeof(message.payload), path,
                  args, registry) != 0)
    return -EINVAL;
  if (OS1_port_send(port, &message) != (long)sizeof(message))
    return -EIO;
  int pid = nxinit_ack(slot, NXMANAGE_REP_REGISTER);
  if (pid < 0)
    OS1_report_error("nxinit.service_registration", pid);
  /* Manager publishes authoritative status per named service and slot. */
  return pid;
}

/* Parses one immutable manifest line, never changes daemon policy.
 * The separators are mandatory so malformed lines cannot silently change
 * the interpretation of image identity, arguments or registry authority. */
static int nxinit_line(int port, unsigned slot, char *line, int mdebug) {
  char *field[6] = {line, NULL, NULL, NULL, NULL, NULL};
  unsigned split = 1U;
  for (unsigned i = 0U; i < NXINIT_LINE_BYTES && line[i] != '\0'; ++i) {
    if (line[i] == '|') {
      if (split >= 6U)
        return -EINVAL;
      line[i] = '\0';
      field[split++] = line + i + 1U;
    }
  }
  if (split != 6U || field[2][0] == '\0')
    return -EINVAL;
  uint32_t mode = 0U;
  if (strcmp(field[1], "lock") == 0)
    mode = NXMANAGE_MODE_LOCK;
  else if (strcmp(field[1], "once") == 0)
    mode = NXMANAGE_MODE_ONCE;
  else if (strcmp(field[1], "start") == 0)
    mode = NXMANAGE_MODE_START_ONCE;
  else if (strcmp(field[1], "async") == 0)
    mode = NXMANAGE_MODE_ASYNC_ONCE;
  else if (strcmp(field[1], "debug") == 0) {
    if (!mdebug)
      return 0; /* skipped by boot profile, not executed */
    mode = NXMANAGE_MODE_ASYNC_ONCE;
  } else {
    return -EINVAL;
  }
  int result = nxinit_declare(port, slot, mode,
                              field[0], field[2], field[3], field[4],
                              split == 6U ? field[5] : "");
  if (result <= 0 && mode == NXMANAGE_MODE_ONCE)
    return -EIO; /* dependent services must not start after failed bootstrap */
  return result <= 0 ? 1 : 0; /* persistent lock can be retried by manager */
}

static int nxinit_manifest(int port, int mdebug) {
  char manifest[NXINIT_MANIFEST_BYTES];
  int n = file_read(NXMANAGE_SERVICES_CONFIG, manifest,
                    sizeof(manifest) - 1U, 0);
  if (n <= 0 || n >= (int)sizeof(manifest))
    return -EIO;
  manifest[n] = '\0';
  unsigned slot = 0U, start = 0U;
  for (unsigned i = 0U; i < NXINIT_MANIFEST_BYTES; ++i) {
    if (i > (unsigned)n)
      continue;
    if (manifest[i] != '\n' && manifest[i] != '\0')
      continue;
    unsigned length = i - start;
    if (length >= NXINIT_LINE_BYTES)
      return -EINVAL;
    manifest[i] = '\0';
    if (length > 0U && manifest[i - 1U] == '\r')
      manifest[i - 1U] = '\0';
    char *line = manifest + start;
    if (line[0] != '\0' && line[0] != '#') {
      if (slot >= NXMANAGE_SLOTS)
        return -ENOSPC;
      int result = nxinit_line(port, slot, line, mdebug);
      if (result < 0)
        return result;
      ++slot; /* stable slot indices even when a debug case is skipped */
    }
    start = i + 1U;
  }
  return 0;
}

int nxinit_run(const struct nxinit_identity *identity) {
  if (identity == NULL || identity->name == NULL || identity->boot_message == NULL) {
    OS1_report_error("nxinit.identity", -EINVAL);
    return 1;
  }
  if (get_pid() == 1) {
    OS1_report_error("nxinit.machine_pid_denied", -EPERM);
    return 1;
  }
  int mdebug = nxinit_boot_profile();
  if (mdebug < 0)
    return 1;
  int port = nxinit_port_claim();
  if (port < 0) {
    OS1_report_error("nxinit.manager_port", port);
    return 1;
  }
  int manifest = nxinit_manifest(port, mdebug);
  if (manifest != 0)
    OS1_report_error("nxinit.manifest", manifest);
  long closed = OS1low_handle_close(port);
  if (closed < 0)
    OS1_report_error("nxinit.manager_handle_close", (int)closed);
  if (manifest != 0 || closed < 0)
    return 1;
  if (OS1_registry_set("system.init.name", identity->name) != 0) {
    OS1_report_error("nxinit.identity_publish", -EIO);
    return 1;
  }
  OS1_sleep(1000);
  char notify_pid[24] = {0};
  if (OS1_registry_object_get("srv.notify_pid", notify_pid,
                               sizeof(notify_pid)) == 0) {
    int delivered = OS1_notify_post("System", identity->boot_message);
    if (delivered != 0)
      OS1_report_error("nxinit.boot_notification", delivered);
  }
  /* Init owns boot configuration, not subsequent process supervision.
   * The MACHINE PID1 manager continues to own service lifecycles. */
  return 0;
}
