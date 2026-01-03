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

## flash

## Debugging

### Setup .venv for debugging

You can use `uv` tool to setup virtual environment for pigweed debugging.

```bash
uv sync
source ./.venv/bin/activate
```

### Pigweed Console with RPC support

On .venv or pigweed activated terminal, run the following command:

```bash
python -m pw_console \
  --device /dev/ttyACM0 \
  --baudrate 115200 \
  --proto-globs "practice_rpc/service.proto" \
  --token-databases build/zephyr/zephyr.elf
```

In this terminal, you can see the logs from the device decoded by Pigweed logger,
and you can also call RPCs from here.  

### Simple log decoding

For just deconding logs, you can run:

```bash
python3 -m pw_tokenizer.detokenize \
  --device /dev/ttyACM0 \
  --baudrate 115200 \
  build/zephyr/zephyr.elf
```
