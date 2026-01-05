import argparse
import serial
import time
import os
import sys
import logging
from pw_hdlc.rpc import HdlcRpcClient, default_channels
import service_pb2
from service_pb2 import EchoRequest, EchoResponse, LedRequest, LedResponse

logging.getLogger("pw_hdlc").setLevel(logging.DEBUG)
logging.basicConfig(level=logging.INFO)


def get_rpc_client(device="/dev/ttyACM0", baud_rate=115200):
    """
    Connect to the serial device and return the RPC client.

    :param device: device path or name (/dev/ttyACM0, COM3, etc.)
    :param baud_rate: Baud rate for the serial connection
    """
    try:
        ser = serial.Serial(device, baud_rate, timeout=1.0)
    except serial.SerialException as e:
        print(f"Failed to open serial port: {e}")
        return None

    def write(data):
        ser.write(data)

    client = HdlcRpcClient(ser, [service_pb2], default_channels(write))  # type: ignore
    return client.rpcs().practice.rpc.DeviceService


def list_methods(client):
    """List available RPC methods from the client.
    Args:
        client: The RPC client instance.
    """
    print("Available RPC Methods:")
    for method in client:
        print(f"Method: {method.method}, Request: {method.request}, Response: {method.response}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pigweed RPC Client")
    parser.add_argument("--device", "-d", default="/dev/ttyACM0", help="Serial device")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()
    client = get_rpc_client(args.device, args.baud)
    if client is None:
        print("Could not create RPC client.")
        sys.exit(1)
    print(f"Connected to {args.device} succcessfully.")

    list_methods(client)

    print("Sending Echo...")
    # you can run with simple arguments
    status1, response1 = client.Echo(msg="Hello Pigweed!")
    # or you can run with Protobuf message
    status2, response2 = client.Echo(EchoRequest(msg="Hello Pigweed!"))
    if status1.ok():
        print(f"Echo Response: {response1.msg} {response1.ByteSize()}, {status1}")
    else:
        print(f"Echo Failed: {status1}")
    if status2.ok():
        print(f"Echo Response: {response2.msg} {response2.ByteSize()}, {status2}")
    else:
        print(f"Echo Failed: {status2}")

    # Call SetLed ON/OFF
    for i in range(10):
        print("Turning LED ON...")
        status, response = client.SetLed(on=True)
        if status.ok():
            print("LED ON Success")
        else:
            print(f"LED ON Failed: {status}")
        time.sleep(0.5)
        print("Turning LED OFF...")
        status, response = client.SetLed(on=False)
        if status.ok():
            print("LED OFF Success")
        else:
            print(f"LED OFF Failed: {status}")
        time.sleep(0.5)

    os._exit(0)  # Use os._exit to avoid hanging due to background threads
