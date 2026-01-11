#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/settings/settings.h>

#include "pb_decode.h"
#include "pb_encode.h"
#include "practice_rpc/service.rpc.pb.h"
#include "pw_hdlc/decoder.h"
#include "pw_hdlc/default_addresses.h"
#include "pw_hdlc/encoder.h"
#include "pw_hdlc/rpc_channel.h"
#include "pw_log/log.h"
#include "pw_log_tokenized/handler.h"
#include "pw_rpc/server.h"
#include "pw_stream/sys_io_stream.h"

#ifdef __MAIN_EXTERN__   /
  #define EXTERN         
#else
  #define EXTERN extern
#endif

#define LED0_NODE DT_ALIAS(led0)
const struct device* dht22 = DEVICE_DT_GET(DT_ALIAS(dht0));
EXTERN uint32_t dht22_last_read_time = 0;
EXTERN struct net_mgmt_event_callback mgmt_cb;

const int kDht22ReadIntervalMS = 2000;
const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
const struct device* usb_dev = DEVICE_DT_GET(DT_CHOSEN(pigweed_rpc_uart));

EXTERN struct k_mutex write_lock;
EXTERN struct {
  pw::rpc::NanopbServerWriter<practice_rpc_SensorResponse> writer;
  bool active = false;
} sensor_stream_context;
EXTERN ::practice_rpc_WifiSettings wifi_settings{
    .ssid = "YOURSSID",
    .password = "YOURPASSWORD",
};

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

class TcpStreamWriter : public pw::stream::NonSeekableWriter {
 public:
  TcpStreamWriter() : client_fd_(-1) {}
  void set_client_fd(int fd) { client_fd_ = fd; }

 private:
  pw::Status DoWrite(pw::ConstByteSpan data) override {
    if (client_fd_ < 0) return pw::Status::FailedPrecondition();
    ssize_t sent = zsock_send(client_fd_, data.data(), data.size(), 0);
    return (sent == static_cast<ssize_t>(data.size())) ? pw::OkStatus()
                                                       : pw::Status::Unknown();
  }
  int client_fd_;
};

class ThreadSafeHdlcChannelOutput : public pw::hdlc::RpcChannelOutput {
 public:
  ThreadSafeHdlcChannelOutput(pw::stream::Writer& writer, uint8_t address,
                              const char* name)
      : pw::hdlc::RpcChannelOutput(writer, address, name) {}
  // Override Send to make it thread-safe.
  pw::Status Send(pw::ConstByteSpan buffer) override {
    k_mutex_lock(&write_lock, K_FOREVER);
    pw::Status status = pw::hdlc::RpcChannelOutput::Send(buffer);
    k_mutex_unlock(&write_lock);
    return status;
  }
};

namespace practice::rpc {
class DeviceService
    : public pw_rpc::nanopb::DeviceService::Service<DeviceService> {
 public:
  DeviceService() {}

  ::pw::Status SetLed(const ::practice_rpc_LedRequest& request,
                      ::practice_rpc_LedResponse& response) {
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
    memcpy(response.msg, request.msg, sizeof(response.msg));
    response.msg[sizeof(response.msg) - 1] = '\0';
    PW_LOG_INFO("Echo requested: %d", (int)strlen(response.msg));
    return ::pw::OkStatus();
  }
  ::pw::Status GetSensorData(const ::practice_rpc_SensorRequest& request,
                             ::practice_rpc_SensorResponse& response) {
    uint32_t now = k_uptime_get_32();
    if (now - dht22_last_read_time < kDht22ReadIntervalMS) {
      k_msleep(kDht22ReadIntervalMS - (now - dht22_last_read_time));
    }
    dht22_last_read_time = k_uptime_get_32();
    int rc = sensor_sample_fetch(dht22);
    if (rc != 0) {
      PW_LOG_ERROR("Failed to read from sensor %d", rc);
      return ::pw::Status::Internal();
    }
    struct sensor_value temperature, humidity;
    sensor_channel_get(dht22, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
    sensor_channel_get(dht22, SENSOR_CHAN_HUMIDITY, &humidity);
    response.temperature = (float)sensor_value_to_double(&temperature);
    response.humidity = (float)sensor_value_to_double(&humidity);
    PW_LOG_DEBUG("Sensor data: Temp=%.2f C, Humidity=%.2f %%",
                 (double)response.temperature, (double)response.humidity);
    return ::pw::OkStatus();
  }
  void StartSensorStream(
      const practice_rpc_SensorRequest& request,
      pw::rpc::NanopbServerWriter<practice_rpc_SensorResponse>& writer) {
    if (sensor_stream_context.active) {
      writer.Finish();
    }
    sensor_stream_context.writer = std::move(writer);
    sensor_stream_context.active = true;
    PW_LOG_INFO("Sensor streaming started");
  }

  ::pw::Status StopSensorStream(const practice_rpc_Empty& request,
                                practice_rpc_Empty& response) {
    sensor_stream_context.active = false;
    sensor_stream_context.writer.Finish();
    PW_LOG_INFO("Sensor streaming stopped");
    return pw::OkStatus();
  }
  ::pw::Status ConfigureWifi(const practice_rpc_WifiSettings& request,
                             practice_rpc_Empty& response) {
    settings_save_one("wifi/ssid", request.ssid, strlen(request.ssid) + 1);
    settings_save_one("wifi/password", request.password, strlen(request.password) + 1);
    PW_LOG_INFO("Configuring Wi-Fi: SSID=%s", request.ssid);
    return pw::OkStatus();
  }
};
}  // namespace practice::rpc