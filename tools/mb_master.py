"""mb_master.py - Phase 7 Modbus RTU master for the FRDM-MCXN236 slave (step 7.1).

PC side of the 7.0 design: slave address 1, 19200 baud 8E1 over the MCU-Link VCOM (COM3).
Holding registers: 0 g_cnt_a (RO), 1 LED P4_18 (RW, 1 = on), 2/3 tick low/high (RO), 4-7 scratch (RW).

Needs pymodbus 3.16.2 + pyserial 3.5 (C:\\Users\\LENOVO\\Documents\\modbus_venv).
Run with the venv python, for example:
  modbus_venv\\Scripts\\python.exe mb_master.py --port COM3 demo
  modbus_venv\\Scripts\\python.exe mb_master.py --port COM3 read 0 8
  modbus_venv\\Scripts\\python.exe mb_master.py --port COM3 write 1 0
  modbus_venv\\Scripts\\python.exe mb_master.py --port COM3 writem 4 4660 0
  modbus_venv\\Scripts\\python.exe mb_master.py --port COM3 raw "01 03 00 04 00 02"
  modbus_venv\\Scripts\\python.exe mb_master.py --port COM3 raw "01 03 00 04 00 02" --badcrc

"raw" sends exact bytes with pyserial (CRC appended, low byte first, unless --nocrc;
--badcrc flips the last CRC byte) and prints every byte that comes back plus the time from the
end of the request to the first reply byte.
"""
import argparse
import sys
import time

import serial
from pymodbus.client import ModbusSerialClient

BAUD = 19200
SLAVE = 1


def crc16(data):
    """CRC-16/Modbus (Serial Line V1.02 p.39): init 0xFFFF, poly 0xA001, LSB first."""
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def hexs(data):
    return " ".join(f"{b:02X}" for b in data)


def raw(port, text, add_crc=True, bad_crc=False, wait=0.3):
    req = bytearray(int(x, 16) for x in text.split())
    if add_crc:
        c = crc16(req)
        req += bytes([c & 0xFF, c >> 8])          # low byte first (p.15)
        if bad_crc:
            req[-1] ^= 0xFF
    with serial.Serial(port, BAUD, bytesize=8, parity=serial.PARITY_EVEN,
                       stopbits=1, timeout=0) as s:
        s.reset_input_buffer()
        s.write(req)
        s.flush()                                 # returns when the bytes left the PC driver
        t_end = time.perf_counter()
        rx = bytearray()
        t_first = None
        while time.perf_counter() - t_end < wait:
            chunk = s.read(64)
            if chunk:
                if t_first is None:
                    t_first = time.perf_counter()
                rx += chunk
            else:
                time.sleep(0.001)
    print(f"TX  {hexs(req)}")
    if rx:
        ok = len(rx) >= 4 and crc16(rx) == 0
        print(f"RX  {hexs(rx)}   ({len(rx)} B, CRC {'ok' if ok else 'BAD'}, "
              f"first byte {1000 * (t_first - t_end):.1f} ms after flush - PC/USB latency included)")
    else:
        print(f"RX  (nothing in {int(wait * 1000)} ms)")
    return bytes(rx)


def client(port):
    c = ModbusSerialClient(port, baudrate=BAUD, bytesize=8, parity="E", stopbits=1,
                           timeout=0.5, retries=0)
    if not c.connect():
        sys.exit(f"cannot open {port}")
    return c


def show(rr, what):
    if rr.isError():
        code = getattr(rr, "exception_code", None)
        print(f"{what}: EXCEPTION code {code}  ({rr})")
    else:
        regs = getattr(rr, "registers", None)
        if regs:
            print(f"{what}: " + " ".join(f"{v:#06x}" for v in regs))
        else:
            print(f"{what}: ok, address {getattr(rr, 'address', '?')} count {getattr(rr, 'count', '?')}")
    return rr


def call(fn, what):
    try:
        return show(fn(), what)
    except Exception as e:                        # pymodbus raises ModbusIOException on no reply
        print(f"{what}: NO REPLY ({type(e).__name__}: {e})")
        return None


def demo(port):
    c = client(port)
    print("T1 read 0..7")
    call(lambda: c.read_holding_registers(0, count=8, device_id=SLAVE), "   regs")
    print("T2 write reg 1 = 0 (LED off), read back")
    call(lambda: c.write_register(1, 0, device_id=SLAVE), "   write")
    call(lambda: c.read_holding_registers(1, count=1, device_id=SLAVE), "   reg1")
    time.sleep(1.0)
    print("T3 write reg 1 = 1 (LED on)")
    call(lambda: c.write_register(1, 1, device_id=SLAVE), "   write")
    print("T4 write regs 4..5 = 0x1234 0x0000, read back")
    call(lambda: c.write_registers(4, [0x1234, 0x0000], device_id=SLAVE), "   writem")
    call(lambda: c.read_holding_registers(4, count=2, device_id=SLAVE), "   regs4-5")
    print("T5 read reg 8 (does not exist)")
    call(lambda: c.read_holding_registers(8, count=1, device_id=SLAVE), "   reg8")
    print("T6 write reg 0 (read-only)")
    call(lambda: c.write_register(0, 5, device_id=SLAVE), "   write0")
    c.close()
    time.sleep(0.1)
    print("T7 the two 7.0 Q4 requests (regs 4/5 = 0x1234 / 0x0000 from T4)")
    raw(port, "01 03 00 04 00 02")
    raw(port, "01 03 00 08 00 01")
    print("T8 bad CRC (expect no reply)")
    raw(port, "01 03 00 04 00 02", bad_crc=True)
    print("T9 other slave address 2 (expect no reply)")
    raw(port, "02 03 00 04 00 02")
    print("T10 function 04 (not supported)")
    raw(port, "01 04 00 00 00 01")
    print("T11 FC03 quantity 0")
    raw(port, "01 03 00 00 00 00")
    print("T12 broadcast write reg 5 = 0x0055 (expect no reply), then read reg 5")
    raw(port, "00 06 00 05 00 55")
    raw(port, "01 03 00 05 00 01")

def main():
    ap = argparse.ArgumentParser(description="Modbus RTU master for the MCXN236 slave")
    ap.add_argument("--port", default="COM3")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("demo")
    p = sub.add_parser("read"); p.add_argument("addr", type=int); p.add_argument("count", type=int)
    p = sub.add_parser("write"); p.add_argument("addr", type=int); p.add_argument("value", type=int)
    p = sub.add_parser("writem"); p.add_argument("addr", type=int); p.add_argument("values", type=int, nargs="+")
    p = sub.add_parser("raw"); p.add_argument("hex")
    p.add_argument("--nocrc", action="store_true"); p.add_argument("--badcrc", action="store_true")
    a = ap.parse_args()

    if a.cmd == "demo":
        demo(a.port)
    elif a.cmd == "raw":
        raw(a.port, a.hex, add_crc=not a.nocrc, bad_crc=a.badcrc)
    else:
        c = client(a.port)
        if a.cmd == "read":
            call(lambda: c.read_holding_registers(a.addr, count=a.count, device_id=SLAVE), "regs")
        elif a.cmd == "write":
            call(lambda: c.write_register(a.addr, a.value, device_id=SLAVE), "write")
        else:
            call(lambda: c.write_registers(a.addr, a.values, device_id=SLAVE), "writem")
        c.close()


if __name__ == "__main__":
    main()
