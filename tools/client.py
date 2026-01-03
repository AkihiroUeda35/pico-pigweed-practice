import argparse
import serial
import time
import sys
import os
import logging

logging.getLogger("pw_hdlc").setLevel(logging.DEBUG)
logging.basicConfig(level=logging.INFO) 

# Add generated proto to path
sys.path.append(os.path.dirname(__file__))

# Try to import required packages
try:
    from pw_hdlc.rpc import HdlcRpcClient, default_channels
    from pw_status import Status
except ImportError:
    print("Error: Pigweed python packages not found. Please install them:")
    print("pip install pw-hdlc pw-rpc pw-status serial")
    sys.exit(1)

try:
    import service_pb2
except ImportError:
    print("Error: service_pb2 not found. Please generate it.")
    sys.exit(1)

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
    client = HdlcRpcClient(ser, [service_pb2], default_channels(write))
    # Access the service
    service = client.rpcs().practice.rpc.DeviceService
    print(f"Connected to {args.device}")

    # Call Echo
    print("Sending Echo...")
    start_time = time.time()
    # Note: RPC calls are synchronous by default in this client unless async is used.
    # The HdlcRpcClient usually runs a background thread if using the `HdlcRpcClient` helper?
    # Wait, HdlcRpcClient in pw_hdlc.rpc DOES NOT spawn a thread by default for reading?
    # Actually it usually requires a read loop or a background thread.
    # The default HdlcRpcClient implementation starts a background thread to read from the provided read function.
    
    # Let's verify usage. HdlcRpcClient(read, protos, channels)
    # It starts a thread.

    status, response = service.Echo(msg="Hello Pigweed!")
    if status.ok():
        print(f"Echo Response: {response.msg}")
    else:
        print(f"Echo Failed: {status}")

    time.sleep(1)

    # Call SetLed
    print("Turning LED ON...")
    status, response = service.SetLed(on=True)
    if status.ok():
        print("LED ON Success")
    else:
        print(f"LED ON Failed: {status}")

    time.sleep(1)

    print("Turning LED OFF...")
    status, response = service.SetLed(on=False)
    if status.ok():
        print("LED OFF Success")
    else:
        print(f"LED OFF Failed: {status}")
    os._exit(0)  # Use os._exit to avoid hanging due to background threads
       
if __name__ == "__main__":
    main()
