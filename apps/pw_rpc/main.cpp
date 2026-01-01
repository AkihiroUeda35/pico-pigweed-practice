#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>

#include "pw_hdlc/decoder.h"
#include "pw_hdlc/default_addresses.h"
#include "pw_hdlc/rpc_channel.h"
#include "pw_rpc/server.h"
#include "pw_stream/sys_io_stream.h"
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#include "practice_rpc/service.rpc.pb.h"

namespace practice::rpc {
class DeviceService
    : public pw_rpc::nanopb::DeviceService::Service<DeviceService> {
 public:
  ::pw::Status SetLed(const ::practice_rpc_LedRequest& request,
                      ::practice_rpc_LedResponse& response) {
    (void)request;
    (void)response;
    if (request.on) {
      LOG_INF("LED ON requested");
    } else {
      LOG_INF("LED OFF requested");
    }
    return ::pw::OkStatus();
  }
  ::pw::Status Echo(const ::practice_rpc_EchoRequest& request,
                    ::practice_rpc_EchoResponse& response) {
    // Echo back the received message
    response.msg = request.msg;
    return ::pw::OkStatus();
  }
};
}  // namespace practice::rpc

pw::stream::SysIoWriter serial_writer;
pw::hdlc::RpcChannelOutput hdlc_channel_output(serial_writer,
                                               pw::hdlc::kDefaultRpcAddress,
                                               "HDLC_OUT");
pw::rpc::Channel channels[] = {
    pw::rpc::Channel::Create<1>(&hdlc_channel_output)};
pw::rpc::Server server(channels);

practice::rpc::DeviceService device_service;

extern "C" {
int main() {
  LOG_INF("Pico 2 W Wi-Fi Project (C++) Started!");
  if (usb_enable(NULL)) return 0;
  std::array<std::byte, 1024> decode_buffer;
  server.RegisterService(device_service);
  pw::hdlc::Decoder decoder(decode_buffer);

  const struct device* dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

  while (1) {
    uint8_t rx_data[64];
    int n = uart_fifo_read(dev, rx_data, sizeof(rx_data));
    if (n > 0) {
      for (int i = 0; i < n; i++) {
        auto result = decoder.Process(static_cast<std::byte>(rx_data[i]));
        if (result.ok()) {
          server.ProcessPacket(result.value().data());
        }
      }
    }
    k_sleep(K_MSEC(1));
  }
  return 0;
}
}