#include <errno.h>
#include <os1.h>
#include <os1_tty.h>
#include <stdio.h>

int main(void) {
  char port[64] = {0};
  int rc = OS1_env_get(OS1_TTY_ENV, port, sizeof(port));
  if (rc == 0) {
    int fd = OS1_port_open(port);
    rc = fd < 0 ? -errno : (int)OS1low_handle_close(fd);
  }
  int written = printf("[USERKIT][%s] terminal port=%s rc=%d\n",
                       rc == 0 ? "PASS" : "FAIL", port, rc);
  return written < 0 || rc != 0;
}
