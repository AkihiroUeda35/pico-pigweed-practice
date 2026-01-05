# Pico2 Zephyr Pigweed RPC Starter

## Setup pigweed and zephyr

On devcontainer terminal, run the following commands:

```bash
source ./third_party/pigweed/bootstrap.sh
pw package install zephyr
#(on repository init)pw zephyr manifest
west init -l manifest
west update
west blobs fetch hal_infineon # for pico2 w
```

As the output of the last command is saved in the workspace, you can skip this step next time.

## Build

```bash
source ./third_party/pigweed/activate.sh
west build -p -b rpi_pico2/rp2350a/m33/w apps/simple
west build -p -b rpi_pico2/rp2350a/m33/w apps/pw_rpc
# build schema
protoc --python_out=./tools --pyi_out=./tools apps/pw_rpc/proto/service.proto && mv tools/apps/pw_rpc/proto/* tools/
```

## Flash

Flash zephyr.uf2 to the device as shown [here](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html#your-first-binaries).

## Debugging

### Setup .venv for debugging

You can use `uv` tool to setup virtual environment for pigweed debugging.

```bash
uv sync
source ./.venv/bin/activate
```

### Run sample script

You can run echo and led on/off sample scripts as follows:

```bash
python tools/client.py --device /dev/ttyACM0
```
