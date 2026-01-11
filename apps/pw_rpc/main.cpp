#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/settings/settings.h>
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

#define __MAIN_EXTERN__
#include "main.h"

UsbStreamWriter serial_writer(usb_dev);
TcpStreamWriter tcp_writer;
bool wifi_connected = false;
K_SEM_DEFINE(wifi_ready_sem, 0, 1);

#define RPC_PORT 8888
#define RECV_BUF_SIZE 512
void wifi_connect(void) {
  struct net_if* iface = net_if_get_default();
  while (true) {
    net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
    k_sleep(K_SECONDS(1));
    PW_LOG_INFO("Preparing to connect to Wi-Fi: SSID=%s", wifi_settings.ssid);
    struct wifi_connect_req_params params = {
        .ssid = (const uint8_t*)wifi_settings.ssid,
        .ssid_length = strlen(wifi_settings.ssid),
        .psk = (const uint8_t*)wifi_settings.password,
        .psk_length = strlen(wifi_settings.password),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
    };
    PW_LOG_INFO("Requesting Wi-Fi connection...");
    int ret =
        net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
    if (ret != 0 && ret != -120) {
      PW_LOG_ERROR("Initial request failed (ret=%d), retrying...", ret);
      k_sleep(K_SECONDS(2));
      continue;
    }
    //  Waiting for connection
    bool connected = false;
    for (int retry = 0; retry < 5; retry++) {
      struct wifi_iface_status status;
      net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status));
      PW_LOG_INFO("Checking State: %d", status.state);
      if (status.state == 4 || status.state == 9) {  // WIFI_STATE_COMPLETED
        connected = true;
        break;
      }
      k_sleep(K_SECONDS(2));
    }
    if (connected) {
      return;
    }
    PW_LOG_ERROR("Connection timeout, resetting...");
  }
}

void rpc_tcp_server_thread(void* p1, void* p2, void* p3) {
  k_sem_take(&wifi_ready_sem, K_FOREVER);
  PW_LOG_INFO("Starting Wi-Fi connection process");
  wifi_connect();
  // Wait for IP address assignment
  struct net_if* iface = net_if_get_default();
  PW_LOG_INFO("Waiting for IP address...");
  int wait_count = 0;
  while (true) {
    struct in_addr* addr =
        net_if_ipv4_get_global_addr(iface, NET_ADDR_ANY_STATE);
    if (addr != NULL) {
      char buf[INET_ADDRSTRLEN];
      zsock_inet_ntop(AF_INET, addr, buf, sizeof(buf));
      PW_LOG_INFO("IP assigned: %s", buf);
      break;
    }
    if (wait_count % 10 == 0) {
      PW_LOG_INFO("Still waiting for IP address...");
    }
    wait_count++;
    k_sleep(K_MSEC(500));
  }
  int serv = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  struct sockaddr_in bind_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(RPC_PORT),
      .sin_addr = {.s_addr = INADDR_ANY},
  };
  zsock_bind(serv, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
  zsock_listen(serv, 1);

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client =
        zsock_accept(serv, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client >= 0) {
      tcp_writer.set_client_fd(client);
      ThreadSafeHdlcChannelOutput hdlc_channel_output(
          tcp_writer, pw::hdlc::kDefaultRpcAddress, "HDLC_OUT");
      pw::rpc::Channel channels[] = {
          pw::rpc::Channel::Create<1>(&hdlc_channel_output)};
      pw::rpc::Server server(channels);
      practice::rpc::DeviceService device_service;
      server.RegisterService(device_service);
      char client_ip[INET_ADDRSTRLEN];
      zsock_inet_ntop(AF_INET, &client_addr.sin_addr, client_ip,
                      sizeof(client_ip));
      PW_LOG_INFO("Client connected: %s", client_ip);
      wifi_connected = true;

      std::array<std::byte, 1024> decoder_buffer;
      pw::hdlc::Decoder decoder(decoder_buffer);

      while (1) {
        char buf[1024];
        ssize_t len = zsock_recv(client, buf, sizeof(buf), 0);
        PW_LOG_DEBUG("Received %d bytes from client", len);
        if (len <= 0) {
          PW_LOG_INFO("Client disconnected");
          wifi_connected = false;
          break;
        }
        for (int i = 0; i < len; i++) {
          auto result = decoder.Process(static_cast<std::byte>(buf[i]));
          if (result.ok()) {
            server.ProcessPacket(result.value().data());
          }
        }
      }
      zsock_close(client);
    }
  }
}

K_THREAD_DEFINE(rpc_server_tid, 8192, rpc_tcp_server_thread, NULL, NULL, NULL,
                7, 0, 0);

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

extern "C" void pw_log_tokenized_HandleLog(uint32_t metadata,
                                           const uint8_t log_buffer[],
                                           size_t size_bytes) {
  k_mutex_lock(&write_lock, K_FOREVER);
  std::array<std::byte, sizeof(uint32_t)> metadata_bytes =
      pw::bytes::CopyInOrder(pw::endian::little, metadata);
  // Send log with HDLC over USB
  pw::hdlc::Encoder encoder(serial_writer);
  encoder.StartUnnumberedFrame(pw::hdlc::kDefaultLogAddress);
  encoder.WriteData(metadata_bytes);
  encoder.WriteData(pw::as_bytes(pw::span(log_buffer, size_bytes)));
  encoder.FinishFrame();
  // Send log with HDLC over Wi-Fi
  if (wifi_connected) {
    pw::hdlc::Encoder tcp_encoder(tcp_writer);
    tcp_encoder.StartUnnumberedFrame(pw::hdlc::kDefaultLogAddress);
    tcp_encoder.WriteData(metadata_bytes);
    tcp_encoder.WriteData(pw::as_bytes(pw::span(log_buffer, size_bytes)));
    tcp_encoder.FinishFrame();
  }
  k_mutex_unlock(&write_lock);
}

static int handle_wifi(const char* name, size_t len, settings_read_cb read_cb,
                       void* cb_arg) {
  if (strcmp(name, "ssid") == 0) {
    PW_LOG_INFO("Loading Wi-Fi SSID from settings");
    ssize_t ret =
        read_cb(cb_arg, wifi_settings.ssid, sizeof(wifi_settings.ssid));
    return (ret < 0) ? (int)ret : 0;
  }
  if (strcmp(name, "password") == 0) {
    PW_LOG_INFO("Loading Wi-Fi password from settings");
    ssize_t ret =
        read_cb(cb_arg, wifi_settings.password, sizeof(wifi_settings.password));
    return (ret < 0) ? (int)ret : 0;
  }
  return -ENOENT;
}
struct settings_handler wifi_conf = {.name = "wifi", .h_set = handle_wifi};

extern "C" {
int main() {
  PW_LOG_INFO("Pico 2 w with zephyr and pigweed started!");
  if (usb_enable(NULL)) return 0;
  k_mutex_init(&write_lock);
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
  settings_subsys_init();
  settings_register(&wifi_conf);
  settings_load();
  k_sem_give(&wifi_ready_sem);
  
  ThreadSafeHdlcChannelOutput hdlc_channel_output(
      serial_writer, pw::hdlc::kDefaultRpcAddress, "HDLC_OUT");
  pw::rpc::Channel channels[] = {
      pw::rpc::Channel::Create<1>(&hdlc_channel_output)};
  pw::rpc::Server server(channels);
  practice::rpc::DeviceService device_service;
  server.RegisterService(device_service);

  while (1) {
    uint8_t rx_data[64];
    int n = uart_fifo_read(usb_dev, rx_data, sizeof(rx_data));
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