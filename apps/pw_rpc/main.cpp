#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>

#include <iostream>

#include "practice_rpc/service.rpc.pb.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

// DeviceService implementation (dummy)
#include "practice_rpc/service.rpc.pb.h"
#include "practice_rpc/service.rpc.pb.h"  // for pw_rpc::nanopb::DeviceService::Service

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

class WifiManager {
 public:
  WifiManager() { LOG_INF("WifiManager Instance Created"); }

  void check_interface() {
    struct net_if* iface = net_if_get_default();
    if (iface) {
      std::cout << "Network interface is ready (C++ std::cout)" << std::endl;
    } else {
      LOG_ERR("Interface not found");
    }
  }
};

extern "C" {
int main() {
  LOG_INF("Pico 2 W Wi-Fi Project (C++) Started!");

  WifiManager wifi_mgr;
  wifi_mgr.check_interface();

  while (true) {
    k_sleep(K_SECONDS(10));
  }

  return 0;
}
}