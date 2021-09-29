# Copyright Paul Spooren <mail@aparcar.org>
#
# SPDX-License-Identifier: GPL-2.0-or-later

import serial
import sys
import yaml
from pathlib import Path

print(f"{sys.argv[0]} [node_name] [serial_port] [bitrage]")
if len(sys.argv) == 2:
    node_name = sys.argv[1]
else:
    node_name = "develop"

if len(sys.argv) == 3:
    node_serial = sys.argv[2]
else:
    node_serial = "/dev/ttyUSB0"

if len(sys.argv) == 4:
    node_bitrate = int(sys.argv[3])
else:
    node_bitrate = 115200

print(f"Provisioning {node_name} at {node_serial}")

if not Path(node_serial).is_char_device():
    print(f"Missing serial connection for node at {node_serial}")
    quit(1)

config_path = Path(f"./nodes/{node_name}.yml")

if not config_path.is_file():
    print(f"Missing node config at nodes/{node_name}.yml")
    quit(1)

config = yaml.safe_load(config_path.read_text())


def AT_command(key, value):
    serial_connection.write(bytes("\n", "ascii"))
    serial_connection.readline()

    at_command = f"AT+{key}={value}"
    serial_connection.write(bytes(at_command, "ascii"))
    print(at_command)
    status = serial_connection.readline().decode("utf-8")
    print(f"STATUS: {status}")
    if not status.startswith("+OK"):
        print(status)
        sys.exit(1)

    return serial_connection.readline().decode("utf-8")


serial_connection = serial.Serial(node_serial, node_bitrate, timeout=1)

for key, value in config["at_commands"].items():
    if not value:
        value = "0" * 16
    AT_command(key, str(value))

# restart node
AT_command("RESET", 1)

serial_connection.close()
