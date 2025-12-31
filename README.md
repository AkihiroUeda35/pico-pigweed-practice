# Pico2 Zephyr Pigweed RPC Starter

## setup

On devcontainer terminal, run the following commands:

```bash
uv venv
source .venv/bin/activate
west init .
west update 
west zephyr-export
west blobs fetch hal_infineon
```

As the output of the last command is saved in the workspace, you can skip this step next time.

## build

```bash
source .venv/bin/activate
west build -b rpi_pico2/rp2350a/m33/w src
```