# Pico2 Zephyr Pigweed RPC Starter

A boilerplate project for high-performance, reliable embedded systems using **Raspberry Pi Pico 2 (RP2350)**, **Zephyr RTOS**, and **Google Pigweed (pw_rpc)**.

## Overview
This project demonstrates a modern embedded development workflow, integrating Pigweed's robust RPC system with Zephyr's hardware abstraction on the RP2350 M33 core.

## Prerequisites

- **Zephyr SDK**: v0.17.0 or later.
- **West**: Zephyr's meta-tool installed and initialized.
- **Python**: 3.10+ for Pigweed tooling and Nanopb.

## Project Structure

- `src/`: C++ source code including Service implementations.
   - `proto/`: `.proto` definitions for RPC services.
- `tool/`: Tool directory for Python client scripts.
- `thirdparty/`: Pigweed as submodule.

## Getting Started

1. **Initialize West Workspace**
   
   ```bash
   west init -m [https://github.com/your-username/pico2-pigweed-rpc](https://github.com/your-username/pico2-pigweed-rpc) --mr main
   west update
   ```

2. Build the Firmware

```bash
west build -p always -b rpi_pico2/rp2350a/m33 app
```

3. Flash

Flashing via USB Mass Storage:

```bash
cp build/zephyr/zephyr.uf2 /media/$USER/RPI-RP2/
```

or using openocd.

```bash
west flash
```

## Development Workflow

Define Service: Edit proto/service.proto.

Generate Code: Pigweed's CMake functions automatically invoke protoc and nanopb.

Implement Logic: Inherit from the generated C++ base class in src/service_impl.cc.

Python Client: Use Pigweed's Python tools to call RPCs over Serial/USB.

## Features
Reliable Communication: Checksum-verified RPC packets.

Thread Safety: Integrated with Zephyr's RTOS primitives.

No Malloc: Static memory allocation via Nanopb.