# Pico2 Zephyr Pigweed RPC Starter

## setup

On devcontainer terminal, run the following commands:

```bash
source ./third_party/pigweed/bootstrap.sh
pw package install zephyr
#(on repository init)pw zephyr manifest
west init -l manifest
west update
west blobs fetch hal_infineon
```

As the output of the last command is saved in the workspace, you can skip this step next time.

## build

```bash
source ./third_party/pigweed/activate.sh
rm -rf build && west build -b rpi_pico2/rp2350a/m33/w apps/simple
rm -rf build && west build -b rpi_pico2/rp2350a/m33/w apps/pw_rpc
```