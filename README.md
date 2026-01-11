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
west build -p -b rpi_pico2/rp2350a/m33/w apps/pw_rpc # pristine build
west build -b rpi_pico2/rp2350a/m33/w apps/pw_rpc # not pristine build
```
# build python schema
protoc \
  -I ./apps/pw_rpc \
  -I ./modules/lib/nanopb/generator/proto \
  --python_out=./tools \
  --pyi_out=./tools \
  ./apps/pw_rpc/service.proto \
  ./modules/lib/nanopb/generator/proto/nanopb.proto
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

### PW console

You can use `pw console` command to open pigweed console.

```bash
pw console --device /dev/ttyACM0
```