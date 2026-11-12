#include <nxdemo1init.h>
#include <nxinit_runtime.h>

int main(void) {
  const struct nxinit_identity identity = {
      .name = NXDEMO1INIT_NAME,
      .boot_message = NXDEMO1INIT_BOOT_MESSAGE,
  };
  return nxinit_run(&identity);
}
