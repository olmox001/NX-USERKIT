/* Capability-PORT echo service used to validate USERKIT module interchange. */
#include <nxexample_echo.h>
#include <os1.h>
#include <string.h>
#include <unistd.h>

enum { NXEXAMPLE_ECHO_REQUESTS = 256 };

static int reply_to_request(struct os1_port_packet *packet) {
  int reply = -1;
  if (packet == NULL || packet->cap_count != 1U ||
      packet->message.type != IPC_TYPE_RAW)
    return -EINVAL;
  unsigned char *bytes = (unsigned char *)&reply;
  for (unsigned i = 0U; i < sizeof(reply); ++i)
    bytes[i] = (unsigned char)packet->message.payload[i];
  if (reply < 0)
    return -EINVAL;
  const size_t offset = sizeof(int);
  const char *text = packet->message.payload + offset;
  size_t length = strnlen(text, sizeof(packet->message.payload) - offset);
  long written = write(reply, text, length);
  int closed = close(reply);
  packet->cap_count = 0U;
  if (written != (long)length)
    return written < 0 ? (int)written : -EIO;
  return closed == 0 ? 0 : -EIO;
}

static int serve(int port) {
  for (unsigned request = 0U; request < NXEXAMPLE_ECHO_REQUESTS; ++request) {
    struct os1_port_packet packet;
    int status = OS1_port_packet_receive(port, &packet, 0);
    if (status != 0)
      return status;
    status = reply_to_request(&packet);
    if (status != 0) {
      int closed = OS1_port_packet_close_caps(&packet);
      if (closed != 0)
        return closed;
      OS1_report_error("nxexample_echo.request", status);
    }
  }
  return 0;
}

int main(void) {
  int port = OS1_port_create(NXEXAMPLE_ECHO_PORT);
  if (port < 0)
    return 1;
  int status = serve(port);
  long closed = OS1low_handle_close(port);
  return status == 0 && closed == 0 ? 0 : 1;
}
