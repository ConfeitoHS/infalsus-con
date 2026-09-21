"""Post-build: convert firmware.hex into firmware.uf2 for drag-and-drop
flashing on the Adafruit/nice!nano UF2 bootloader.

Addresses come from the Intel HEX file, so the app lands at its linked
address (0x26000 for S140 v6) and never overwrites the SoftDevice.
"""
import struct

Import("env")

UF2_MAGIC0 = 0x0A324655
UF2_MAGIC1 = 0x9E5D5157
UF2_MAGIC2 = 0x0AB16F30
FAMILY_NRF52840 = 0xADA52840
FLAG_FAMILY_ID = 0x00002000
BLOCK = 256


def parse_hex(path):
    mem = {}
    base = 0
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line.startswith(":"):
                continue
            data = bytes.fromhex(line[1:])
            count, addr, rtype = data[0], (data[1] << 8) | data[2], data[3]
            payload = data[4:4 + count]
            if rtype == 0x00:
                for i, b in enumerate(payload):
                    mem[base + addr + i] = b
            elif rtype == 0x04:
                base = ((payload[0] << 8) | payload[1]) << 16
            elif rtype == 0x02:
                base = ((payload[0] << 8) | payload[1]) << 4
            elif rtype == 0x01:
                break
    return mem


def to_uf2(mem):
    addrs = sorted(mem)
    blocks = []
    i = 0
    while i < len(addrs):
        start = addrs[i] & ~(BLOCK - 1)
        chunk = bytearray(BLOCK)
        used = False
        for off in range(BLOCK):
            a = start + off
            if a in mem:
                chunk[off] = mem[a]
                used = True
        if used:
            blocks.append((start, bytes(chunk)))
        # advance past this block
        while i < len(addrs) and addrs[i] < start + BLOCK:
            i += 1
    out = bytearray()
    total = len(blocks)
    for n, (addr, chunk) in enumerate(blocks):
        hdr = struct.pack("<IIIIIIII", UF2_MAGIC0, UF2_MAGIC1, FLAG_FAMILY_ID,
                          addr, BLOCK, n, total, FAMILY_NRF52840)
        out += hdr + chunk + bytes(476 - BLOCK) + struct.pack("<I", UF2_MAGIC2)
    return bytes(out)


def make_uf2(source, target, env):
    hex_path = str(target[0])
    if not hex_path.endswith(".hex"):
        hex_path = env.subst("$BUILD_DIR/${PROGNAME}.hex")
    uf2_path = hex_path[:-4] + ".uf2"
    mem = parse_hex(hex_path)
    with open(uf2_path, "wb") as f:
        f.write(to_uf2(mem))
    lo, hi = min(mem), max(mem)
    print("UF2: %s  (0x%05X-0x%05X, %d bytes)" % (uf2_path, lo, hi, hi - lo + 1))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", make_uf2)
