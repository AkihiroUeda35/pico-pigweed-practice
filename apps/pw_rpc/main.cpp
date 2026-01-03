#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>

#include "practice_rpc/service.rpc.pb.h"
#include "pw_hdlc/decoder.h"
#include "pw_hdlc/default_addresses.h"
#include "pw_hdlc/rpc_channel.h"
#include "pw_log/log.h"
#include "pw_rpc/server.h"
#include "pw_stream/sys_io_stream.h"

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

namespace practice::rpc {
class DeviceService
    : public pw_rpc::nanopb::DeviceService::Service<DeviceService> {
 public:
  DeviceService() {
    if (!gpio_is_ready_dt(&led)) {
      return;
    }

    if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE) < 0) {
      return;
    }
  }
  ::pw::Status SetLed(const ::practice_rpc_LedRequest& request,
                      ::practice_rpc_LedResponse& response) {
    (void)request;
    (void)response;
    if (request.on) {
      PW_LOG_INFO("LED ON requested");
      gpio_pin_set_dt(&led, 1);
    } else {
      PW_LOG_INFO("LED OFF requested");
      gpio_pin_set_dt(&led, 0);
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

class UsbStreamWriter : public pw::stream::NonSeekableWriter {
 public:
  UsbStreamWriter(const struct device* dev) : dev_(dev) {}

 private:
  pw::Status DoWrite(pw::ConstByteSpan data) override {
    for (std::byte b : data) {
      uart_poll_out(dev_, static_cast<uint8_t>(b));
    }
    return pw::OkStatus();
  }
  const struct device* dev_;
};

extern "C" {
int main() {
  PW_LOG_INFO("Pico 2 w with zephyr and pigweed started!");
  if (usb_enable(NULL)) return 0;
  const struct device* dev = DEVICE_DT_GET(DT_CHOSEN(pigweed_rpc_uart));

  std::array<std::byte, 1024> decode_buffer;
  pw::hdlc::Decoder decoder(decode_buffer);

  UsbStreamWriter serial_writer(dev);
  pw::hdlc::RpcChannelOutput hdlc_channel_output(
      serial_writer, pw::hdlc::kDefaultRpcAddress, "HDLC_OUT");
  pw::rpc::Channel channels[] = {
      pw::rpc::Channel::Create<1>(&hdlc_channel_output)};
  pw::rpc::Server server(channels);
  practice::rpc::DeviceService device_service;
  server.RegisterService(device_service);

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
    } else {
      k_sleep(K_MSEC(1));
    }
  }
  return 0;
}
}