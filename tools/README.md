# Tools

This directory contains the Python client for the RPC service.

## Setup

1.  **Environment Setup**:
    We use `uv` to manage the Python environment. Run the following in the project root:
    ```bash
    uv sync
    ```

2.  **Generate Protobuf Python code**:
    Generate `service_pb2.py` from the proto definition using `uv run` and `protoc`.
    ```bash
    # From project root
    uv run protoc -Isrc/proto --python_out=tools src/proto/service.proto
    ```

## Usage

Ensure your device is connected (e.g., Raspberry Pi Pico 2 via USB).

```bash
# From project root
uv run tools/client.py --device /dev/ttyACM0
```

### Options
- `--device`, `-d`: Serial device path (default: `/dev/ttyACM0`)
- `--baud`, `-b`: Baud rate (default: `115200`)
