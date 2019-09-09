"""
Common things for talking to the Midi-Fader via hidraw
"""

import hid
import struct

VID = 0x16c0
PID = 0x05dc

class Command(object):
    """
    Basic command structure: 4 bytes of command followed by up to 60 bytes of data
    """
    def __init__(self, command, data_bytes):
        self.command = command
        self.data_bytes = data_bytes

    def pack(self):
        return struct.pack('<I60s', self.command, self.data_bytes)

class StatusCommand(Command):
    """
    Command to get the device status
    """
    def __init__(self):
        super().__init__(0x00, b'')

class GetCommand(Command):
    """
    Command to get a parameter
    """
    def __init__(self, parameter):
        super().__init__(0x40, struct.pack('<IIII', 0, parameter, 0, 0))

class SetCommand(Command):
    """
    Command to set a parameter
    """
    def __init__(self, parameter, value, size):
        super().__init__(0x80, struct.pack('<IIII', 0, parameter, value, size))

class EnterBootloaderCommand(Command):
    """
    Command to enter the bootloader
    """
    def __init__(self):
        super().__init__(0x0C, b'')

class Response(object):
    """
    Basic response structure: 4 bytes of command followed by up to 60 bytes of data
    """
    def __init__(self, data_bytes):
        unpacked = struct.unpack('<I60s', bytes(data_bytes))
        self.command = unpacked[0]
        self.bytes = unpacked[1]
        self.parameters = struct.unpack('<15i', self.bytes)

    @property
    def status(self):
        return self.parameters[0]

class GetCommandResponse(Response):
    @property
    def value(self):
        full_bytes = struct.pack('<i', self.parameters[2])
        if self.size == 1:
            return struct.unpack('<b', full_bytes[:1])[0]
        elif self.size == 2:
            return struct.unpack('<h', full_bytes[:2])[0]
        else:
            return self.parameters[2]

    @property
    def size(self):
        return self.parameters[3]

class Device(hid.device):
    MANUFACTURER='kevincuzner.com'
    PRODUCT='Midi-Fader'
    def __init__(self, path):
        self.path = path

    def __enter__(self):
        self.open_path(self.path)
        return self

    def __exit__(self, *args):
        self.close()

    def status(self):
        cmd = StatusCommand()
        return Response(self.write_command(cmd))

    def get_parameter(self, parameter):
        cmd = GetCommand(parameter)
        return GetCommandResponse(self.write_command(cmd))

    def set_parameter(self, parameter, value, size):
        cmd = SetCommand(parameter, value, size)
        return Response(self.write_command(cmd))

    def write_command(self, command, response=True):
        data = b'\x00' + command.pack() #prepend a zero since we don't use REPORT_ID
        result = self.write(data)
        if result < 0:
            raise ValueError(self.error())
        if not response:
            return None
        response = self.read(64)
        if len(response):
            return response

    def enter_bootloader(self):
        cmd = EnterBootloaderCommand()
        self.write_command(cmd, response=False)

def find_device(cls=Device):
    info = hid.enumerate(VID, PID)
    for i in info:
        if i['manufacturer_string'] == cls.MANUFACTURER and\
                i['product_string'] == cls.PRODUCT:
            return cls(i['path'])
    return None

