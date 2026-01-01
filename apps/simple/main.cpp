#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>

#include <iostream>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

class WifiManager {
 public:
  WifiManager() { LOG_INF("WifiManager Instance Created"); }

  void check_interface() {
    struct net_if* iface = net_if_get_default();
    if (iface) {
      LOG_INF("Network interface is ready (Zephyr log)");
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
  int i = 0;
  while (true) {
    k_sleep(K_SECONDS(1000));
    LOG_INF("Main loop iteration %d", i);
    i++;
  }

  return 0;
}
}