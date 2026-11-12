/* Example interchangeable notification backend for USERKIT demos. */
#include <nxnotify_service.h>
#include <os1.h>
#include <stdio.h>
#include <string.h>

enum { NXNOTIFY_EXAMPLE_REQUESTS = 256 };

static int publish_notification(const struct ipc_message *message) {
  char count_text[24];
  static unsigned count;
  if (message == NULL || message->type != IPC_TYPE_NOTIFY)
    return -EINVAL;
  ++count;
  if (OS1_registry_set("example.notify.last", message->payload) != 0)
    return -EIO;
  int n = snprintf(count_text, sizeof(count_text), "%u", count);
  if (n <= 0 || n >= (int)sizeof(count_text))
    return -EOVERFLOW;
  if (OS1_registry_set("example.notify.count", count_text) != 0)
    return -EIO;
  return 0;
}

static int serve(int port) {
  for (unsigned request = 0U; request < NXNOTIFY_EXAMPLE_REQUESTS; ++request) {
    struct os1_port_packet packet;
    int status = OS1_port_packet_receive(port, &packet, 0);
    if (status != 0)
      return status;
    status = publish_notification(&packet.message);
    int closed = OS1_port_packet_close_caps(&packet);
    if (status != 0)
      OS1_report_error("nxnotify_registry.request", status);
    if (closed != 0)
      return closed;
  }
  return 0;
}

int main(void) {
  int port = OS1_port_create(OS1NX_PORT_NOTIFY);
  if (port < 0) {
    OS1_report_error("nxnotify_registry.port", port);
    return 1;
  }
  int status = serve(port);
  long closed = OS1low_handle_close(port);
  if (status != 0)
    OS1_report_error("nxnotify_registry.serve", status);
  return status == 0 && closed == 0 ? 0 : 1;
}
