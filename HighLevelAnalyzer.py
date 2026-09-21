from saleae.analyzers import HighLevelAnalyzer, AnalyzerFrame


class Transaction:
    command_id = None
    command_name = "unknown"
    expects_response = True
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
        self.command_stop_end_time = None
        self.byte_errors = []

    def command_data(self):
        return {}

    def response_data(self):
        return {}

    # Runs an unpacker over a transfer that may have arrived short, in which case the
    # transfer is reported as the error it is rather than raising out of the analyzer
    def unpack(self, unpacker, what):
        try:
            data = unpacker()
        except (IndexError, ValueError):
            return None, f"truncated {self.command_name} {what}"

        # Every byte the low level analyzer had to guess at rides along with the transfer
        # that carried it, so the count starts again for the next one
        if self.byte_errors:
            data["error"] = ", ".join(self.byte_errors)

        self.byte_errors = []

        return data, None

    def to_command_frame(self):
        data, error = self.unpack(self.command_data, "command")
        if error is not None:
            return error_frame(error, self.command_start_time, self.command_end_time)

        return AnalyzerFrame(
            type=f"{self.command_name}_tx",
            start_time=self.command_start_time,
            end_time=self.command_end_time,
            data=data,
        )

    def to_response_frame(self):
        data, error = self.unpack(self.response_data, "response")
        if error is not None:
            return error_frame(error, self.response_start_time, self.response_end_time)

        return AnalyzerFrame(
            type=f"{self.command_name}_rx",
            start_time=self.response_start_time,
            end_time=self.response_end_time,
            data=data,
        )


# One bad state, shown as its own frame so it stands out from the transfers around it
def error_frame(reason, start_time, end_time):
    return AnalyzerFrame(
        type="error",
        start_time=start_time,
        end_time=end_time,
        data={"error": reason},
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
    expects_response = False


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


class MgmtIdentify(Transaction):
    command_id = 0x60
    command_name = "mgmt_identify"

    def command_data(self):
        magic = int.from_bytes(self.command_args[:2], "big")

        return {
            "magic": f"0x{magic:04X}",
        }

    def response_data(self):
        magic = int.from_bytes(self.response[:2], "big")

        return {
            "magic": f"0x{magic:04X}",
            "vendor": f"0x{self.response[2]:02X}",
            "model": f"0x{self.response[3]:02X}",
            "variant": f"0x{self.response[4]:02X}",
            "version": f"{self.response[5]}.{self.response[6]}.{self.response[7]}",
        }


class MgmtCtrl(Transaction):
    command_id = 0x61
    command_name = "mgmt_ctrl"

    def command_data(self):
        return {
            "group": f"0x{self.command_args[0]:02X}",
            "verb": f"0x{self.command_args[1]:02X}",
            "arg": f"0x{self.command_args[2]:02X}",
        }

    def response_data(self):
        return {
            "result": f"0x{self.response[0]:02X}",
        }


class MgmtStatus(Transaction):
    command_id = 0x62
    command_name = "mgmt_status"

    def command_data(self):
        return {
            "group": f"0x{self.command_args[0]:02X}",
        }

    def response_data(self):
        return {
            "status": self.response.hex(" ").upper(),
        }


class MgmtConfigRead(Transaction):
    command_id = 0x63
    command_name = "mgmt_config_read"

    def command_data(self):
        return {
            "group": f"0x{self.command_args[0]:02X}",
            "block": f"0x{self.command_args[1]:02X}",
        }

    def response_data(self):
        return {
            "data": self.response.hex(" ").upper(),
        }


class MgmtConfigWrite(Transaction):
    command_id = 0x64
    command_name = "mgmt_config_write"

    def command_data(self):
        return {
            "group": f"0x{self.command_args[0]:02X}",
            "block": f"0x{self.command_args[1]:02X}",
            "data": self.command_args[2:].hex(" ").upper(),
        }

    def response_data(self):
        return {
            "result": f"0x{self.response[0]:02X}",
        }


class MgmtDataWrite(Transaction):
    command_id = 0x65
    command_name = "mgmt_data_write"

    def command_data(self):
        address = int.from_bytes(self.command_args[1:3], "big")

        return {
            "group": f"0x{self.command_args[0]:02X}",
            "address": f"0x{address:04X}",
        }

    def response_data(self):
        return {
            "checksum": f"0x{self.response[0]:02X}",
        }


class JoybusHla(HighLevelAnalyzer):
    transaction = None

    result_types = {
        "error": {"format": "Error: {{data.error}}"},
    }

    # A command is only known to have gone unanswered once something else turns up on
    # the bus, so the check runs against whatever arrives next
    def check_unanswered(self, end_time):
        if self.transaction is None or self.transaction.command_end_time is None:
            return None

        if self.transaction.response:
            return error_frame(
                f"truncated {self.transaction.command_name} response",
                self.transaction.response_start_time,
                end_time,
            )

        if not self.transaction.expects_response:
            return None

        return error_frame(
            f"{self.transaction.command_name} not answered",
            self.transaction.command_stop_end_time,
            end_time,
        )

    def decode(self, frame: AnalyzerFrame):
        frames = []

        # A run the low level analyzer could not read says nothing about what was in
        # flight, so the transaction around it is abandoned rather than stitched across
        if frame.type == "error":
            frames.append(self.check_unanswered(frame.start_time))
            frames.append(error_frame(frame.data["error"], frame.start_time, frame.end_time))
            self.transaction = None
            return [f for f in frames if f is not None]

        if frame.type == "byte":
            data_type = frame.data["type"]
            data_byte = frame.data["data"][0]

            # An opcode always starts a new transaction. Arguments only belong to one
            # that has not stopped yet, and response bytes only to one that has, so
            # bytes left over from a transaction the capture began in the middle of
            # are dropped rather than folded into the transaction before them.
            if data_type == "command":
                frames.append(self.check_unanswered(frame.start_time))
                self.transaction = Transaction.create(data_byte)
                self.transaction.command_start_time = frame.start_time
            elif data_type == "argument":
                if self.transaction is None or self.transaction.command_end_time is not None:
                    self.transaction = None
                    return None
                self.transaction.command_args.append(data_byte)
            elif data_type == "response":
                if self.transaction is None or self.transaction.command_end_time is None:
                    self.transaction = None
                    return None
                if self.transaction.response_start_time is None:
                    self.transaction.response_start_time = frame.start_time
                self.transaction.response.append(data_byte)
            else:
                return None

            self.transaction.last_byte_end_time = frame.end_time

            # A byte the low level analyzer had to guess at taints the transfer it lands in
            if "error" in frame.data:
                self.transaction.byte_errors.append(frame.data["error"])

        # The stop bit says which end was driving the line, so a capture that starts
        # part way through a transaction still lands its frames on the right side
        if frame.type == "stop" and self.transaction:
            if frame.data["type"] == "host":
                if self.transaction.command_end_time is not None:
                    self.transaction = None
                    return None
                self.transaction.command_end_time = self.transaction.last_byte_end_time
                self.transaction.command_stop_end_time = frame.end_time
                frames.append(self.transaction.to_command_frame())
            else:
                if self.transaction.command_end_time is None:
                    self.transaction = None
                    return None
                self.transaction.response_end_time = self.transaction.last_byte_end_time

                # A target that answers a command nothing should answer is as much a bad
                # state as one that stays quiet when it should not
                if self.transaction.expects_response:
                    frames.append(self.transaction.to_response_frame())
                else:
                    frames.append(
                        error_frame(
                            f"{self.transaction.command_name} answered",
                            self.transaction.response_start_time,
                            self.transaction.response_end_time,
                        )
                    )

                # The transaction is complete, so nothing after it belongs here
                self.transaction = None

        # A bad stop bit leaves the low level analyzer reporting both a stop and an error
        if frame.type == "stop" and "error" in frame.data:
            frames.append(error_frame(frame.data["error"], frame.start_time, frame.end_time))

        return [f for f in frames if f is not None]
