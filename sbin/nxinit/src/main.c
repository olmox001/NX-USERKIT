#include <nxinit.h>
#include <nxinit_runtime.h>

int main(void) {
  const struct nxinit_identity identity = {
      .name = NXINIT_NAME,
      .boot_message = NXINIT_BOOT_MESSAGE,
  };
  return nxinit_run(&identity);
}
