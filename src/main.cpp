#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>

#include "pw_log/log.h"
#include "pw_rpc/server.h"
#include "pw_hdlc/rpc_channel.h"
#include "pw_hdlc/default_addresses.h"
#include "pw_hdlc/decoder.h"
#include "pw_stream/sys_io_stream.h"
#include "proto/service.pwpb.h"
#include "proto/service.rpc.pwpb.h"

namespace {

using practice::rpc::pw_rpc::pwpb::DeviceService;

class DeviceServiceImpl final : public DeviceService::Service<DeviceServiceImpl> {
public:
  pw::Status SetLed(const practice::rpc::pwpb::LedRequest::Message& request,
                    practice::rpc::pwpb::LedResponse::Message& response) {
    bool on = request.on;
    if (on) {
        gpio_pin_set_dt(&led, 1);
    } else {
        gpio_pin_set_dt(&led, 0);
    }
    return pw::OkStatus();
  }

  pw::Status Echo(const practice::rpc::pwpb::EchoRequest::Message& request,
                  practice::rpc::pwpb::EchoResponse::Message& response) {
    // Echo implementation requires handling callbacks for strings in pwpb.
    // For now, we just return OK status.
    return pw::OkStatus();
  }
  
  void InitLed() {
    if (!gpio_is_ready_dt(&led)) {
        return;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    gpio_pin_set_dt(&led, 0);
  }

private:
  static const struct gpio_dt_spec led;
};

const struct gpio_dt_spec DeviceServiceImpl::led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

// RPC Setup
pw::stream::SysIoWriter writer;
pw::hdlc::RpcChannelOutput hdlc_channel_output(writer, pw::hdlc::kDefaultRpcAddress, "HDLC");
pw::rpc::Channel channels[] = { pw::rpc::Channel::Create<1>(&hdlc_channel_output) };
pw::rpc::Server server(channels);
DeviceServiceImpl device_service;

} // namespace

int main() {
    // Initialize USB
    if (usb_enable(NULL)) {
        return 0;
    }

    // Initialize LED
    device_service.InitLed();

    // Register Service
    server.RegisterService(device_service);

    // PW_LOG_INFO("Ready for RPC");

    // Main Loop: Read Byte -> Process
    pw::hdlc::DecoderBuffer<256> hdlc_decoder;
    while (true) {
        std::byte data;
        if (pw::sys_io::ReadByte(&data).ok()) {
            auto result = hdlc_decoder.Process(data);
            if (result.ok()) {
                pw::hdlc::Frame frame = result.value();
                if (frame.address() == pw::hdlc::kDefaultRpcAddress) {
                    server.ProcessPacket(frame.data());
                }
            }
        }
    }
    return 0;
}
