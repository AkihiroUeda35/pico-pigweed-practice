import argparse
import sys
import logging
import threading
import serial
from client import get_rpc_client
import service_pb2
from service_pb2 import EchoRequest, EchoResponse, LedRequest, LedResponse
from pw_console.embed import PwConsoleEmbed
from pw_console.log_store import LogStore


def start_serial_log_thread(log_store: LogStore, device="/dev/ttyUSB0", baud=115200):
    """Start a background thread to read lines from a serial port and log to log_store."""
    # Create a logger for device logs
    device_logger = logging.getLogger("pw_console.device_log")
    device_logger.setLevel(logging.INFO)
    device_logger.addHandler(log_store)

    def serial_reader():
        try:
            ser = serial.Serial(device, baud, timeout=1.0)
            while True:
                line = ser.readline()
                if line:
                    # Decode and strip line endings
                    msg = line.decode(errors="replace").rstrip()
                    device_logger.info(msg)
        except Exception as e:
            device_logger.error(f"Serial log thread error: {e}")

    t = threading.Thread(target=serial_reader, daemon=True)
    t.start()


def main():
    parser = argparse.ArgumentParser(description="Pigweed pw_console embedded RPC Console")
    parser.add_argument("--device", "-d", default="/dev/ttyACM0", help="Serial device")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate")

    parser.add_argument("--log-device", "-l", default="/dev/ttyUSB0", help="Serial device for logs")
    parser.add_argument("--log-baud", "-r", type=int, default=115200, help="Baud rate for logs")

    args = parser.parse_args()

    client = get_rpc_client(args.device, args.baud)
    if client is None:
        print("Could not create RPC client.")
        sys.exit(1)

    # LogStore for device logs
    device_log_store = LogStore()
    start_serial_log_thread(device_log_store, device=args.log_device, baud=args.log_baud)

    loggers = {
        "Host Logs": [logging.getLogger(__package__), logging.getLogger(__name__)],
        "Device Logs": logging.getLogger("pw_console.device_log"),
    }

    # Provide useful globals for the REPL
    global_vars = {
        **globals(),
        "client": client,
        "EchoRequest": EchoRequest,
        "EchoResponse": EchoResponse,
        "LedRequest": LedRequest,
        "LedResponse": LedResponse,
        "service_pb2": service_pb2,
    }

    console = PwConsoleEmbed(
        global_vars=global_vars,
        local_vars=locals(),
        loggers=loggers,
        app_title="Pigweed RPC pw_console",
        repl_startup_message='Type: client.Echo(msg="Hello") or client.SetLed(on=True)',
    )
    console.setup_python_logging()
    console.embed()


if __name__ == "__main__":
    main()
