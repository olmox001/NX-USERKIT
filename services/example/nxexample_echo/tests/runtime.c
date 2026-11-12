#include <nxexample_echo.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  char reply[32] = {0};
  if (NX_example_echo("USERKIT-ECHO", reply, sizeof(reply)) != 0)
    return 1;
  if (strcmp(reply, "USERKIT-ECHO") != 0)
    return 2;
  int written = printf("[USERKIT][PASS] echo=%s\n", reply);
  return written > 0 ? 0 : 3;
}
