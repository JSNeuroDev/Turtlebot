import struct
import socket


class Turtle:
    def __init__(self):
        self.tcpConnection = socket.socket()
        self.tcpConnection.connect(('192.168.50.242', 80))

    def go(self, steps=1):
        """Move the turtle forward by 'steps' units. Default is 1."""
        wheel_deg = steps * 37.2
        command = b'G' + b'\x00'  # command 'G' for 'Go' and '0 = Forwards'
        cmd_msg = command + struct.pack('f', wheel_deg)  # convert wheelDeg to bytes
        self.tcpConnection.sendall(cmd_msg)
        confirm = self.tcpConnection.recv(1)

    def turn(self, degrees=90):
        """Turn the turtle by 'degrees'. Default is 90 degrees."""
        wheel_deg = degrees * 2.97
        command = b'G' + b'\x01'  # command 'G' for 'Go' and '1 = Turn'
        cmd_msg = command + struct.pack('f', wheel_deg)
        self.tcpConnection.sendall(cmd_msg)
        confirm = self.tcpConnection.recv(1)

    def stop(self):
        cmd_msg = struct.pack('b', ord('X'))
        self.tcpConnection.sendall(cmd_msg)
        confirm = self.tcpConnection.recv(1)

    def wait(self, seconds=1):
        command = b'W'  # command 'W' for wait (seconds)
        cmd_msg = command + struct.pack('f', seconds*1000)
        self.tcpConnection.sendall(cmd_msg)
        confirm = self.tcpConnection.recv(1)

    def speed(self, speed=0):
        command = b'V'  # command 'V' to set velocity
        cmd_msg = command + struct.pack('f', speed)  # convert speed to bytes
        self.tcpConnection.sendall(cmd_msg)
        confirm = self.tcpConnection.recv(1)
