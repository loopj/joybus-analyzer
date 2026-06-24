from saleae.analyzers import HighLevelAnalyzer, AnalyzerFrame


class Transaction:
    command_id = None
    command_name = "unknown"
    registry = {}

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        if cls.command_id is not None:
            Transaction.registry[cls.command_id] = cls

    @classmethod
    def create(cls, command_byte):
        transaction_cls = cls.registry.get(command_byte, Transaction)
        return transaction_cls()

    def __init__(self):
        self.command_args = bytearray()
        self.response = bytearray()
        self.command_start_time = None
        self.command_end_time = None
        self.response_start_time = None
        self.response_end_time = None
        self.last_byte_end_time = None

    def command_data(self):
        return {}

    def response_data(self):
        return {}

    def to_command_frame(self):
        return AnalyzerFrame(
            type=f"{self.command_name}_tx",
            start_time=self.command_start_time,
            end_time=self.command_end_time,
            data=self.command_data(),
        )

    def to_response_frame(self):
        return AnalyzerFrame(
            type=f"{self.command_name}_rx",
            start_time=self.response_start_time,
            end_time=self.response_end_time,
            data=self.response_data(),
        )


class Reset(Transaction):
    command_id = 0xFF
    command_name = "reset"

    def response_data(self):
        type = int.from_bytes(self.response[:2], "big")
        status = self.response[2]

        return {
            "type": f"0x{type:04X}",
            "status": f"0x{status:02X}",
        }


class Identify(Transaction):
    command_id = 0x00
    command_name = "identify"

    def response_data(self):
        type = int.from_bytes(self.response[:2], "big")
        status = self.response[2]

        return {
            "type": f"0x{type:04X}",
            "status": f"0x{status:02X}",
        }


class N64Read(Transaction):
    command_id = 0x01
    command_name = "n64_read"

    def response_data(self):
        buttons = int.from_bytes(self.response[:2], "big")
        stick_x = self.response[2]
        stick_y = self.response[3]

        return {
            "buttons": f"0x{buttons:04X}",
            "stick_x": f"0x{stick_x:02X}",
            "stick_y": f"0x{stick_y:02X}",
        }


class N64AccessoryRead(Transaction):
    command_id = 0x02
    command_name = "n64_accessory_read"

    def command_data(self):
        address = int.from_bytes(self.command_args[:2], "big")

        return {
            "address": f"0x{address:04X}",
        }

    def response_data(self):
        return {
            "checksum": f"0x{self.response[32]:02X}",
        }


class N64AccessoryWrite(Transaction):
    command_id = 0x03
    command_name = "n64_accessory_write"

    def command_data(self):
        address = int.from_bytes(self.command_args[:2], "big")

        return {
            "address": f"0x{address:04X}",
        }

    def response_data(self):
        return {
            "checksum": f"0x{self.response[0]:02X}",
        }


class N64EepromRead(Transaction):
    command_id = 0x04
    command_name = "n64_eeprom_read"

    def command_data(self):
        return {
            "block": f"0x{self.command_args[0]:02X}",
        }


class N64EepromWrite(Transaction):
    command_id = 0x05
    command_name = "n64_eeprom_write"

    def command_data(self):
        return {
            "block": f"0x{self.command_args[0]:02X}",
        }

    def response_data(self):
        return {
            "status": f"0x{self.response[0]:02X}",
        }


class N64RtcInfo(Transaction):
    command_id = 0x06
    command_name = "n64_rtc_info"

    def response_data(self):
        type = int.from_bytes(self.response[:2], "big")
        status = self.response[2]

        return {
            "type": f"0x{type:04X}",
            "status": f"0x{status:02X}",
        }


class N64RtcRead(Transaction):
    command_id = 0x07
    command_name = "n64_rtc_read"

    def command_data(self):
        return {
            "block": f"0x{self.command_args[0]:02X}",
        }

    def response_data(self):
        return {
            "status": f"0x{self.response[8]:02X}",
        }


class N64RtcWrite(Transaction):
    command_id = 0x08
    command_name = "n64_rtc_write"

    def command_data(self):
        return {
            "block": f"0x{self.command_args[0]:02X}",
        }

    def response_data(self):
        return {
            "status": f"0x{self.response[0]:02X}",
        }


class N64KeyboardRead(Transaction):
    command_id = 0x13
    command_name = "n64_keyboard_read"


class GBARead(Transaction):
    command_id = 0x14
    command_name = "gba_read"


class GBAWrite(Transaction):
    command_id = 0x15
    command_name = "gba_write"


class PixelFXGameID(Transaction):
    command_id = 0x1D
    command_name = "pixelfx_game_id"


class GCNRead(Transaction):
    command_id = 0x40
    command_name = "gcn_read"

    def command_data(self):
        analog_mode = self.command_args[0]
        motor_state = self.command_args[1]

        return {
            "analog_mode": f"0x{analog_mode:02X}",
            "motor_state": f"0x{motor_state:02X}",
        }

    def response_data(self):
        analog_mode = self.command_args[0]

        buttons = int.from_bytes(self.response[:2], "big")
        stick_x = self.response[2]
        stick_y = self.response[3]

        data = {
            "buttons": f"0x{buttons:04X}",
            "stick_x": f"0x{stick_x:02X}",
            "stick_y": f"0x{stick_y:02X}",
        }

        # Unpack remaining analog input data based on mode
        if analog_mode == 1:
            # Triggers full precision, substick X/Y and analog A/B truncated to 4 bits
            data["substick_x"] = f"0x{self.response[4] & 0xF0:02X}"
            data["substick_y"] = f"0x{(self.response[4] & 0x0F) << 4:02X}"
            data["trigger_left"] = f"0x{self.response[5]:02X}"
            data["trigger_right"] = f"0x{self.response[6]:02X}"
            data["analog_a"] = f"0x{self.response[7] & 0xF0:02X}"
            data["analog_b"] = f"0x{(self.response[7] & 0x0F) << 4:02X}"
        elif analog_mode == 2:
            # Analog A/B full precision, substick X/Y and triggers truncated to 4 bits
            data["substick_x"] = f"0x{self.response[4] & 0xF0:02X}"
            data["substick_y"] = f"0x{(self.response[4] & 0x0F) << 4:02X}"
            data["trigger_left"] = f"0x{self.response[5] & 0xF0:02X}"
            data["trigger_right"] = f"0x{(self.response[5] & 0x0F) << 4:02X}"
            data["analog_a"] = f"0x{self.response[6]:02X}"
            data["analog_b"] = f"0x{self.response[7]:02X}"
        elif analog_mode == 3:
            # Substick X/Y and triggers full precision, analog A/B omitted;
            data["substick_x"] = f"0x{self.response[4]:02X}"
            data["substick_y"] = f"0x{self.response[5]:02X}"
            data["trigger_left"] = f"0x{self.response[6]:02X}"
            data["trigger_right"] = f"0x{self.response[7]:02X}"
        elif analog_mode == 4:
            # Substick X/Y and analog A/B full precision, triggers omitted
            data["substick_x"] = f"0x{self.response[4]:02X}"
            data["substick_y"] = f"0x{self.response[5]:02X}"
            data["analog_a"] = f"0x{self.response[6]:02X}"
            data["analog_b"] = f"0x{self.response[7]:02X}"
        else:
            # Substick X/Y full precision, triggers and analog A/B truncated to 4 bits
            data["substick_x"] = f"0x{self.response[4]:02X}"
            data["substick_y"] = f"0x{self.response[5]:02X}"
            data["trigger_left"] = f"0x{self.response[6] & 0xF0:02X}"
            data["trigger_right"] = f"0x{(self.response[6] & 0x0F) << 4:02X}"
            data["analog_a"] = f"0x{self.response[7] & 0xF0:02X}"
            data["analog_b"] = f"0x{(self.response[7] & 0x0F) << 4:02X}"

        return data


class GCNReadOrigin(Transaction):
    command_id = 0x41
    command_name = "gcn_read_origin"

    def response_data(self):
        buttons = int.from_bytes(self.response[:2], "big")

        return {
            "buttons": f"0x{buttons:04X}",
            "stick_x": f"0x{self.response[2]:02X}",
            "stick_y": f"0x{self.response[3]:02X}",
            "substick_x": f"0x{self.response[4]:02X}",
            "substick_y": f"0x{self.response[5]:02X}",
            "trigger_left": f"0x{self.response[6]:02X}",
            "trigger_right": f"0x{self.response[7]:02X}",
            "analog_a": f"0x{self.response[8]:02X}",
            "analog_b": f"0x{self.response[9]:02X}",
        }


class GCNCalibrate(GCNReadOrigin):
    command_id = 0x42
    command_name = "gcn_calibrate"

    def response_data(self):
        buttons = int.from_bytes(self.response[:2], "big")

        return {
            "buttons": f"0x{buttons:04X}",
            "stick_x": f"0x{self.response[2]:02X}",
            "stick_y": f"0x{self.response[3]:02X}",
            "substick_x": f"0x{self.response[4]:02X}",
            "substick_y": f"0x{self.response[5]:02X}",
            "trigger_left": f"0x{self.response[6]:02X}",
            "trigger_right": f"0x{self.response[7]:02X}",
            "analog_a": f"0x{self.response[8]:02X}",
            "analog_b": f"0x{self.response[9]:02X}",
        }


class GCNReadLong(Transaction):
    command_id = 0x43
    command_name = "gcn_read_long"

    def command_data(self):
        analog_mode = self.command_args[0]
        motor_state = self.command_args[1]

        return {
            "analog_mode": f"0x{analog_mode:02X}",
            "motor_state": f"0x{motor_state:02X}",
        }

    def response_data(self):
        buttons = int.from_bytes(self.response[:2], "big")

        return {
            "buttons": f"0x{buttons:04X}",
            "stick_x": f"0x{self.response[2]:02X}",
            "stick_y": f"0x{self.response[3]:02X}",
            "substick_x": f"0x{self.response[4]:02X}",
            "substick_y": f"0x{self.response[5]:02X}",
            "trigger_left": f"0x{self.response[6]:02X}",
            "trigger_right": f"0x{self.response[7]:02X}",
            "analog_a": f"0x{self.response[8]:02X}",
            "analog_b": f"0x{self.response[9]:02X}",
        }


class GCNProbeDevice(Transaction):
    command_id = 0x4D
    command_name = "gcn_probe_device"


class GCNFixDevice(Transaction):
    command_id = 0x4E
    command_name = "gcn_fix_device"

    def command_data(self):
        wireless_id = int.from_bytes(self.command_args[:2], "big")

        return {
            "wireless_id": f"0x{wireless_id:04X}",
        }

    def response_data(self):
        type = int.from_bytes(self.response[:2], "big")
        status = self.response[2]

        return {
            "type": f"0x{type:04X}",
            "status": f"0x{status:02X}",
        }


class GCNKeyboardRead(Transaction):
    command_id = 0x54
    command_name = "gcn_keyboard_read"


class JoybusHla(HighLevelAnalyzer):
    transaction = None

    def decode(self, frame: AnalyzerFrame):
        new_frame = None

        if frame.type == "byte":
            data_type = frame.data["type"]
            data_byte = frame.data["data"][0]

            if data_type == "command":
                self.transaction = Transaction.create(data_byte)
                self.transaction.command_start_time = frame.start_time
            elif data_type == "argument" and self.transaction:
                self.transaction.command_args.append(data_byte)
            elif data_type == "response" and self.transaction:
                if self.transaction.response_start_time is None:
                    self.transaction.response_start_time = frame.start_time
                self.transaction.response.append(data_byte)

            self.transaction.last_byte_end_time = frame.end_time

        if frame.type == "stop" and self.transaction:
            if self.transaction.command_end_time is None:
                self.transaction.command_end_time = self.transaction.last_byte_end_time
                new_frame = self.transaction.to_command_frame()
            else:
                self.transaction.response_end_time = self.transaction.last_byte_end_time
                new_frame = self.transaction.to_response_frame()

        return new_frame
