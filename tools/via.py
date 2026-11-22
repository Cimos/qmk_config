# VIA backup/restore and bootloader tool for the MadMan Corne.
# Needs: pip install hidapi. Run on the PC the keyboard is plugged into.
#   python via.py backup FILE | restore FILE | show | bootloader
import sys, json, time, hid
VID, PID = 0x4653, 0x0002
def open_dev():
    for d in hid.enumerate(VID, PID):
        if d['usage_page'] == 0xFF60 and d['usage'] == 0x61:
            h = hid.device(); h.open_path(d['path']); return h
    sys.exit("keyboard raw HID interface not found")
def cmd(h, data):
    buf = bytes(data) + bytes(32 - len(data))
    h.write(b'\x00' + buf)
    r = bytes(h.read(32, 1000))
    if not r or r[0] != data[0]: sys.exit(f"bad reply to {data[0]:#x}: {r[:4]}")
    return r
def read_buf(h, c, size):
    out = b''
    for off in range(0, size, 28):
        n = min(28, size - off)
        out += cmd(h, [c, off >> 8, off & 0xFF, n])[4:4+n]
    return out
def write_buf(h, c, data):
    for off in range(0, len(data), 28):
        chunk = data[off:off+28]
        cmd(h, [c, off >> 8, off & 0xFF, len(chunk)] + list(chunk))
ROWS, COLS = 8, 6
RGB = {1:'brightness', 2:'effect', 3:'speed', 4:'color'}
def snapshot(h):
    s = {}
    s['protocol'] = int.from_bytes(cmd(h, [0x01])[1:3], 'big')
    s['layers'] = cmd(h, [0x11])[1]
    s['keymap'] = read_buf(h, 0x12, s['layers']*ROWS*COLS*2).hex()
    s['macro_count'] = cmd(h, [0x0C])[1]
    msize = int.from_bytes(cmd(h, [0x0D])[1:3], 'big')
    s['macros'] = read_buf(h, 0x0E, msize).hex()
    rgb = {}
    for vid, name in RGB.items():
        r = cmd(h, [0x08, 3, vid])  # custom_get_value, channel 3 = rgb_matrix
        rgb[name] = list(r[3:5])
    s['rgb_matrix'] = rgb
    return s
h = open_dev()
op = sys.argv[1]
if op == 'backup':
    s = snapshot(h); json.dump(s, open(sys.argv[2], 'w'), indent=1)
    print(f"protocol {s['protocol']:#x}, {s['layers']} layers, macros {len(s['macros'])//2} bytes, rgb {s['rgb_matrix']}")
elif op == 'restore':
    s = json.load(open(sys.argv[2])); cur = snapshot(h)
    assert cur['layers'] == s['layers'], "layer count differs"
    if cur['keymap'] != s['keymap']: write_buf(h, 0x13, bytes.fromhex(s['keymap'])); print("keymap restored")
    else: print("keymap already matches")
    mb = bytes.fromhex(s['macros']); size = len(cur['macros'])//2
    assert len(mb.rstrip(b'\0')) <= size, "macros do not fit"
    mb = (mb + bytes(size))[:size]
    if bytes.fromhex(cur['macros']) != mb: write_buf(h, 0x0F, mb); print("macros restored")
    s['macros'] = mb.hex()
    for vid, name in RGB.items():
        v = s['rgb_matrix'][name]
        if abs(cur['rgb_matrix'][name][0]-v[0])>1 or cur['rgb_matrix'][name][1]!=v[1]: cmd(h, [0x07, 3, vid] + v); print(f"rgb {name} restored")
    cmd(h, [0x09, 3])  # custom_save rgb_matrix
    after = snapshot(h)
    print("VERIFY:", "OK" if after['keymap']==s['keymap'] and after['macros']==s['macros'] else "MISMATCH")
elif op == 'bootloader':
    # Needs the 2026-11 firmware or later; older builds ignore this.
    try: cmd(h, [0x0B, ord('B'), ord('L')])
    except SystemExit: pass
    print("bootloader requested (firmware older than 2026-11 ignores this)")
elif op == 'show':
    s = snapshot(h); print(f"protocol {s['protocol']:#x}, layers {s['layers']}, rgb {s['rgb_matrix']}")
