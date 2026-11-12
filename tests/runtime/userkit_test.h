#ifndef NEXSOS_USERKIT_TEST_H
#define NEXSOS_USERKIT_TEST_H
#include <nxmanage.h>
#include <limits.h>
#include <os1.h>
#include <stdio.h>
#include <string.h>

enum { USERKIT_RECORD_FIELDS = 4, USERKIT_RECORD_BYTES = 64 };

static int userkit_parse_uint(const char *text, unsigned *value,
                              unsigned *consumed) {
  unsigned result = 0U;
  unsigned index = 0U;
  if (text == NULL || value == NULL || consumed == NULL)
    return -EINVAL;
  for (; index < USERKIT_RECORD_BYTES && text[index] >= '0' &&
         text[index] <= '9'; ++index) {
    unsigned digit = (unsigned)(text[index] - '0');
    if (result > (UINT_MAX - digit) / 10U)
      return -EOVERFLOW;
    result = result * 10U + digit;
  }
  if (index == 0U)
    return -EINVAL;
  *value = result;
  *consumed = index;
  return 0;
}

static int userkit_parse_record(const char *record, int *pid,
                                unsigned *generation, unsigned *mode,
                                unsigned *status) {
  unsigned values[USERKIT_RECORD_FIELDS] = {0U, 0U, 0U, 0U};
  unsigned offset = 0U;
  if (record == NULL || pid == NULL || generation == NULL || mode == NULL ||
      status == NULL)
    return -EINVAL;
  for (unsigned field = 0U; field < USERKIT_RECORD_FIELDS; ++field) {
    unsigned used = 0U;
    int parsed = userkit_parse_uint(record + offset, &values[field], &used);
    if (parsed != 0)
      return parsed;
    offset += used;
    if (offset >= USERKIT_RECORD_BYTES)
      return -EOVERFLOW;
    if (field + 1U < USERKIT_RECORD_FIELDS) {
      if (record[offset] != ':')
        return -EINVAL;
      ++offset;
    } else if (record[offset] != '\0') {
      return -EINVAL;
    }
  }
  if (values[0] > (unsigned)INT_MAX)
    return -EOVERFLOW;
  *pid = (int)values[0];
  *generation = values[1];
  *mode = values[2];
  *status = values[3];
  return 0;
}

static int userkit_test_service_state(const char *name, unsigned expected_mode,
                                      unsigned expected_status,
                                      int require_live_pid) {
  char key[USERKIT_RECORD_BYTES];
  char record[USERKIT_RECORD_BYTES] = {0};
  int pid = 0;
  unsigned generation = 0U;
  unsigned mode = 0U;
  unsigned status = 0U;
  int n = snprintf(key, sizeof(key), "system.manage.service.%s", name);
  if (n <= 0 || n >= (int)sizeof(key))
    return 2;
  if (OS1_registry_get(key, record, sizeof(record)) != 0) {
    int printed = printf("[USERKIT][FAIL] %s registry unavailable\n", name);
    return printed < 0 ? 6 : 3;
  }
  if (userkit_parse_record(record, &pid, &generation, &mode, &status) != 0) {
    int printed = printf("[USERKIT][FAIL] %s malformed=%s\n", name, record);
    return printed < 0 ? 6 : 4;
  }
  if (generation == 0U || mode != expected_mode || status != expected_status ||
      (require_live_pid != 0 && pid <= 0)) {
    int printed = printf("[USERKIT][FAIL] %s pid=%d generation=%u mode=%u status=%u "
                         "expected_mode=%u expected_status=%u\n",
                         name, pid, generation, mode, status,
                         expected_mode, expected_status);
    return printed < 0 ? 6 : 5;
  }
  int printed = printf("[USERKIT][PASS] %s pid=%d generation=%u mode=%u status=%u\n",
                       name, pid, generation, mode, status);
  return printed < 0 ? 6 : 0;
}

static int userkit_test_lock_service(const char *name) {
  return userkit_test_service_state(name, NXMANAGE_MODE_LOCK, 1U, 1);
}

static int userkit_test_start_service(const char *name) {
  return userkit_test_service_state(name, NXMANAGE_MODE_START_ONCE, 1U, 1);
}

static int userkit_test_once_service(const char *name) {
  return userkit_test_service_state(name, NXMANAGE_MODE_ONCE, 3U, 0);
}

#endif
