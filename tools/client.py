import argparse
import serial
import time
import os
import sys
import logging
import re
import socket
from pw_hdlc.rpc import HdlcRpcClient, default_channels
import service_pb2
from service_pb2 import EchoRequest, EchoResponse, LedRequest, LedResponse, SensorResponse
from pw_tokenizer import detokenize
import threading
import struct
from pw_log_tokenized import Metadata, FormatStringWithMetadata
from communication_utils import find_serial_by_vid_pid, TcpSocketWrapper

logging.getLogger("pw_hdlc").setLevel(logging.INFO)
logging.getLogger("pw_rpc.callback_client").setLevel(logging.INFO)
device_log = logging.getLogger("device")

logging.basicConfig(level=logging.INFO)

logger = logging.getLogger("client")


def get_rpc_client(
    serial_port="",
    baud_rate=115200,
    vid_pid="2fe3:0100",
    ip_address="",
    port=8888,
    elf_path="build/zephyr/zephyr.elf",
):
    """
    Connect to the serial device and return the RPC client.
    Args:
        serial_port: The serial port to connect to. If empty, will search by VID:
        baud_rate: The baud rate for the serial connection.
        vid_pid: The USB VID:PID to search for if serial_port is not provided.
        ip_address: The IP address to connect to via TCP. If provided, will use TCP
        port: The TCP port to connect to.
        elf_path: The path to the ELF file for detokenization.
    Returns:
        The RPC client instance.
    """
    if ip_address:
        logger.info(f"Connecting to TCP {ip_address}:{port}...")
        while True:
            try:
                transport = TcpSocketWrapper(ip_address, port)
                logger.info("Connected via TCP")
                break
            except Exception as e:
                logger.info(f"Failed to connect to {ip_address}: {e}, retrying...")
                time.sleep(1)
    else:
        while True:
            actual_port = serial_port
            if not actual_port:
                logger.info(f"Searching for VID:PID {vid_pid}...")
                actual_port = find_serial_by_vid_pid(vid_pid)

            if actual_port:
                try:
                    transport = serial.Serial(actual_port, baud_rate, timeout=0.1)
                    logger.info(f"Connected via Serial: {actual_port}")
                    break
                except serial.SerialException as e:
                    logger.info(f"Failed to open serial port: {e}")
            time.sleep(1)

    def write(data):
        transport.write(data)

    detokenizer = detokenize.Detokenizer(os.path.realpath(elf_path))  # "build/zephyr/zephyr.elf"

    def detoken(data: bytes):
        result = detokenizer.detokenize(data[4:])
        text = str(result)
        metadata = Metadata(struct.unpack("<I", data[:4])[0])
        msg_match = re.search(r"msg♦(.*?)■", text)
        file_match = re.search(r"file♦(.*?)($|■)", text)
        message = msg_match.group(1) if msg_match else text
        log_level = metadata.log_level * 10
        if log_level > logging.CRITICAL:
            log_level = logging.CRITICAL
        if log_level < logging.DEBUG:
            log_level = logging.DEBUG
        if file_match:
            full_path = file_match.group(1)
            filename = os.path.basename(full_path)
            device_log.log(log_level, f"[{filename}:{metadata.line}] {message}")
            logging.DEBUG
        else:
            device_log.error(f"unknown log {text}")

    client = HdlcRpcClient(transport, [service_pb2], default_channels(write), output=detoken)  # type: ignore

    return client.rpcs().practice.rpc.DeviceService


def list_methods(client):
    """List available RPC methods from the client.
    Args:
        client: The RPC client instance.
    """
    logger.info("Available RPC Methods:")
    result = []
    for method in client:
        logger.info(
            f"Method: {method.method}, Request: {method.request}, Response: {method.response}"
        )
        result.append(
            f"Method: {method.method}, Request: {method.request}, Response: {method.response}"
        )
    return result


def stream_listener_thread(client):
    """Listen to the sensor data stream and print received data."""

    def stream_listener(client):
        try:
            logger.info("Starting stream listener thread...")
            call = client.StartSensorStream.invoke()
            for response in call.get_responses():
                logger.info(
                    f"Streaming response: Temp={response.temperature:.2f} C, Humidity={response.humidity:.2f} %"
                )
        except Exception as e:
            logger.info(f"Stream error: {e}")

    logger.info("Starting Sensor Stream...")
    listener_thread = threading.Thread(target=stream_listener, args=(client,), daemon=True)
    logger.info("Start listener thread OK")
    listener_thread.start()
    return listener_thread


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pigweed RPC Client")
    parser.add_argument("--ip", "-i", default="", help="IP address")
    parser.add_argument("--serial", "-s", default="", help="Serial device")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()
    client = get_rpc_client(ip_address=args.ip, serial_port=args.serial, baud_rate=args.baud)
    if client is None:
        logger.info("Could not create RPC client.")
        sys.exit(1)
    logger.info(f"Connected to device succcessfully.")
    list_methods(client)
    logger.info("Getting Sensor Data...")
    _, response3 = client.GetSensorData()
    logger.info(f"Sensor Data: Temperature={response3.temperature}, Humidity={response3.humidity}")

    logger.info("Starting Sensor Stream...")
    listener_thread = stream_listener_thread(client)

    logger.info("Sending Echo...")
    # you can run with simple arguments
    status1, response1 = client.Echo(msg="Hello Pigweed!")
    # or you can run with Protobuf message
    status2, response2 = client.Echo(EchoRequest(msg="Hello Pigweed!"))
    if status1.ok():
        logger.info(f"Echo Response: {response1.msg} {response1.ByteSize()}, {status1}")
    else:
        logger.info(f"Echo Failed: {status1}")
    if status2.ok():
        logger.info(f"Echo Response: {response2.msg} {response2.ByteSize()}, {status2}")
    else:
        logger.info(f"Echo Failed: {status2}")
    # Call SetLed ON/OFF
    for i in range(10):
        logger.info("Turning LED ON...")
        status, response = client.SetLed(on=True)
        if status.ok():
            logger.info("LED ON Success")
        else:
            logger.info(f"LED ON Failed: {status}")
        time.sleep(0.5)
        logger.info("Turning LED OFF...")
        status, response = client.SetLed(on=False)
        if status.ok():
            logger.info("LED OFF Success")
        else:
            logger.info(f"LED OFF Failed: {status}")
        time.sleep(0.5)
    client.StopSensorStream()
    listener_thread.join(timeout=1)

    os._exit(0)  # Use os._exit to avoid hanging due to background threads
