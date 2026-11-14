# Download this file with uPyCraft. Do not use Burn Firmware.
# Logitech LIGHTSPEED receiver: VID 046D, PID C547
import km
km.mode(1)
km.echo(1)

import device
device.VID(0x046D)
device.PID(0xC547)
device.enable(1)
