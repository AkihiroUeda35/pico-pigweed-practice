#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>

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

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
const struct device* dev = DEVICE_DT_GET(DT_CHOSEN(pigweed_rpc_uart));
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

UsbStreamWriter serial_writer(dev);

const struct device* dht22 = DEVICE_DT_GET(DT_ALIAS(dht0));
static uint32_t dht22_last_read_time = 0;
const int kDht22ReadIntervalMS = 2000;

namespace practice::rpc {
struct {
  pw::rpc::NanopbServerWriter<practice_rpc_SensorResponse> writer;
  bool active = false;
} sensor_stream_context;

void sensor_thread_main(void*, void*, void*) {
  while (true) {
    if (sensor_stream_context.active) {
      practice_rpc_SensorResponse response;
      uint32_t now = k_uptime_get_32();
      if (now - dht22_last_read_time < kDht22ReadIntervalMS) {
        k_msleep(kDht22ReadIntervalMS - (now - dht22_last_read_time));
      }
      dht22_last_read_time = k_uptime_get_32();
      if (sensor_sample_fetch(dht22) == 0) {
        struct sensor_value temp, hum;
        sensor_channel_get(dht22, SENSOR_CHAN_AMBIENT_TEMP, &temp);
        sensor_channel_get(dht22, SENSOR_CHAN_HUMIDITY, &hum);

        response.temperature = (float)sensor_value_to_double(&temp);
        response.humidity = (float)sensor_value_to_double(&hum);
        PW_LOG_DEBUG(
            "Writing sensor stream response: Temp=%.2f C, Humidity=%.2f %%",
            (double)response.temperature, (double)response.humidity);
        if (!sensor_stream_context.writer.Write(response).ok()) {
          PW_LOG_ERROR("Failed to write sensor stream response");
          sensor_stream_context.active = false;
        }
      }
    }
    k_msleep(kDht22ReadIntervalMS);
  }
}

K_THREAD_DEFINE(sensor_tid, 1024, sensor_thread_main, NULL, NULL, NULL, 7, 0,
                0);

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
};
}  // namespace practice::rpc
struct k_mutex usb_write_lock;

extern "C" void pw_log_tokenized_HandleLog(uint32_t metadata,
                                           const uint8_t log_buffer[],
                                           size_t size_bytes) {
  k_mutex_lock(&usb_write_lock, K_FOREVER);
  std::array<std::byte, sizeof(uint32_t)> metadata_bytes =
      pw::bytes::CopyInOrder(pw::endian::little, metadata);
  pw::hdlc::Encoder encoder(serial_writer);
  encoder.StartUnnumberedFrame(pw::hdlc::kDefaultLogAddress);
  encoder.WriteData(metadata_bytes);
  encoder.WriteData(pw::as_bytes(pw::span(log_buffer, size_bytes)));
  encoder.FinishFrame();
  k_mutex_unlock(&usb_write_lock);
}

class ThreadSafeHdlcChannelOutput : public pw::hdlc::RpcChannelOutput {
 public:
  ThreadSafeHdlcChannelOutput(pw::stream::Writer& writer, uint8_t address,
                              const char* name)
      : pw::hdlc::RpcChannelOutput(writer, address, name) {}
  // Override Send to make it thread-safe.
  pw::Status Send(pw::ConstByteSpan buffer) override {
    k_mutex_lock(&usb_write_lock, K_FOREVER);
    pw::Status status = pw::hdlc::RpcChannelOutput::Send(buffer);
    k_mutex_unlock(&usb_write_lock);
    return status;
  }
};

extern "C" {
int main() {
  PW_LOG_INFO("Pico 2 w with zephyr and pigweed started!");
  if (usb_enable(NULL)) return 0;
  k_mutex_init(&usb_write_lock);
  std::array<std::byte, 1024> decode_buffer;
  pw::hdlc::Decoder decoder(decode_buffer);
  if (!device_is_ready(dht22)) {
    printk("Sensor: device not ready.\n");
  }
  sensor_sample_fetch(dht22);
  dht22_last_read_time = k_uptime_get_32();
  if (!gpio_is_ready_dt(&led)) {
    printk("LED: device not ready.\n");
  }
  if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_LOW) < 0) {
    printk("LED: device not configured.\n");
  }
  ThreadSafeHdlcChannelOutput hdlc_channel_output(
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