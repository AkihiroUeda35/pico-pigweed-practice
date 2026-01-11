import serial.tools.list_ports
import socket


class TcpSocketWrapper:
    """A simple TCP socket wrapper to mimic serial.Serial interface for RPC communication."""

    def __init__(self, ip, port, timeout=1.0):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(timeout)
        self.sock.connect((ip, port))

    def read(self, num_bytes: int = 1):
        try:
            return self.sock.recv(num_bytes)
        except socket.timeout:
            return b""

    def write(self, data: bytes):
        return self.sock.sendall(data)

    def close(self):
        self.sock.close()


def find_serial_by_vid_pid(vid_pid: str):
    """Find a serial port whose USB VID:PID matches the given string (e.g. '2fe3:0100').
    On Windows, also checks device description and friendly name for VID:PID pattern."""
    vid, pid = vid_pid.split(":")
    vid = vid.lower()
    pid = pid.lower()
    pattern = f"vid_{vid}&pid_{pid}"
    for port in serial.tools.list_ports.comports():
        # Try direct match using pyserial's parsed vid/pid
        if port.vid is not None and port.pid is not None:
            if f"{port.vid:04x}" == vid and f"{port.pid:04x}" == pid:
                return port.device
        # On Windows, try matching in description or hardware id
        desc = (port.description or "").lower()
        hwid = (port.hwid or "").lower()
        name = (getattr(port, "name", "") or "").lower()
        if pattern in desc or pattern in hwid or pattern in name:
            return port.device
    return None
