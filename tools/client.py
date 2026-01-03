import argparse
import serial
import time
import os
import logging
from pw_hdlc.rpc import HdlcRpcClient, default_channels
import service_pb2
from service_pb2 import EchoRequest, EchoResponse, LedRequest, LedResponse

logging.getLogger("pw_hdlc").setLevel(logging.DEBUG)
logging.basicConfig(level=logging.INFO)


def main():
    parser = argparse.ArgumentParser(description="Pigweed RPC Client")
    parser.add_argument("--device", "-d", default="/dev/ttyACM0", help="Serial device")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()

    # Connect to serial
    try:
        ser = serial.Serial(args.device, args.baud, timeout=0.01)
    except serial.SerialException as e:
        print(f"Failed to open serial port: {e}")
        return

    # Initialize RPC Client
    def read():
        bts = ser.read_all()
        return bts if bts else bytes([])

    def write(data):
        ser.write(data)

    # default_channels uses the writer to send HDLC frames
    client = HdlcRpcClient(ser, [service_pb2], default_channels(write))  # dtype: ignore
    # Access the service
    service = client.rpcs().practice.rpc.DeviceService
    print(f"Connected to {args.device}")
    # List available methods and their request/response types
    for method in service:
        # print(dir(method))
        print(method.method, method.request, method.response)
    # Call Echo
    print("Sending Echo...")

    # you can run with simple arguments
    status1, response1 = service.Echo(msg="Hello Pigweed!")
    # or you can run with Protobuf message
    status2, response2 = service.Echo(EchoRequest(msg="Hello Pigweed!"))
    if status1.ok():
        print(f"Echo Response: {response1.msg} {response1.ByteSize()}, {status1}")
    else:
        print(f"Echo Failed: {status1}")
    if status2.ok():
        print(f"Echo Response: {response2.msg} {response2.ByteSize()}, {status2}")
    else:
        print(f"Echo Failed: {status2}")
    time.sleep(1)

    # Call SetLed ON/OFF
    for i in range(10):
        print("Turning LED ON...")
        status, response = service.SetLed(on=True)
        if status.ok():
            print("LED ON Success")
        else:
            print(f"LED ON Failed: {status}")
        time.sleep(0.5)
        print("Turning LED OFF...")
        status, response = service.SetLed(on=False)
        if status.ok():
            print("LED OFF Success")
        else:
            print(f"LED OFF Failed: {status}")
        time.sleep(0.5)

    os._exit(0)  # Use os._exit to avoid hanging due to background threads


if __name__ == "__main__":
    main()
