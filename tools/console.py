import argparse
import sys
import logging
import threading
import serial
from client import get_rpc_client, list_methods, stream_listener_thread
import service_pb2
from service_pb2 import EchoRequest, EchoResponse, LedRequest, LedResponse
from pw_console.embed import PwConsoleEmbed
from pw_console.log_store import LogStore


def main():
    parser = argparse.ArgumentParser(description="Pigweed pw_console embedded RPC Console")
    parser.add_argument("--ip", "-i", default="", help="IP address")
    parser.add_argument("--serial", "-s", default="", help="Serial device")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate")

    args = parser.parse_args()

    client = get_rpc_client(ip_address=args.ip, serial_port=args.serial, baud_rate=args.baud)
    if client is None:
        print("Could not create RPC client.")
        sys.exit(1)

    loggers = {
        "Host Logs": [logging.getLogger("client"), logging.getLogger("pw_rpc.callback_client")],
        "Device Logs": [logging.getLogger("device")],
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
    methods_list = list_methods(client)
    methods_str = "\n".join(methods_list)
    console = PwConsoleEmbed(
        global_vars=global_vars,
        local_vars=locals(),
        loggers=loggers,
        app_title="Pigweed RPC pw_console",
        repl_startup_message=methods_str,
    )
    console.setup_python_logging()
    console.embed()


if __name__ == "__main__":
    main()
