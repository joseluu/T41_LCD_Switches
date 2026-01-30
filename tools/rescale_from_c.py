"""Rescale button from reference C array (RGB565A8 format) to 106x40.
Reads the reference button_3_wip_2.c (116x68, LV_IMG_CF_RGB565A8),
decodes to RGBA, rescales with Pillow, re-encodes as RGB565A8.
"""
from PIL import Image
import re
import os

script_dir = os.path.dirname(os.path.abspath(__file__))
ref_path = os.path.join(os.path.expanduser('~'), 'hobby_w', 'ESP32',
                        'lvgl_button_test5', 'button_3_wip_2.c')
src_dir = os.path.join(script_dir, '..', 'src')

SRC_W, SRC_H = 116, 68
DST_W, DST_H = 106, 40

# Read hex bytes from the reference C file
with open(ref_path, 'r') as f:
    text = f.read()

# Extract all hex values from the array
hex_values = re.findall(r'0x([0-9a-fA-F]{2})', text)
raw = bytes(int(h, 16) for h in hex_values)
print(f"Read {len(raw)} bytes from reference ({SRC_W}x{SRC_H})")

# RGB565A8 layout: first SRC_W*SRC_H*2 bytes = RGB565, then SRC_W*SRC_H bytes = alpha
rgb_size = SRC_W * SRC_H * 2
alpha_offset = rgb_size

# Decode to RGBA image
img = Image.new('RGBA', (SRC_W, SRC_H))
pixels = []
for i in range(SRC_W * SRC_H):
    lo = raw[i * 2]
    hi = raw[i * 2 + 1]
    rgb565 = lo | (hi << 8)
    r = ((rgb565 >> 11) & 0x1F) << 3
    g = ((rgb565 >> 5) & 0x3F) << 2
    b = (rgb565 & 0x1F) << 3
    a = raw[alpha_offset + i]
    pixels.append((r, g, b, a))

img.putdata(pixels)
print(f"Decoded {SRC_W}x{SRC_H} image")

# Rescale
img_resized = img.resize((DST_W, DST_H), Image.LANCZOS)
print(f"Resized to {DST_W}x{DST_H}")

# Re-encode as RGB565A8
rgb_data = bytearray()
alpha_data = bytearray()
for y in range(DST_H):
    for x in range(DST_W):
        r, g, b, a = img_resized.getpixel((x, y))
        r5 = (r >> 3) & 0x1F
        g6 = (g >> 2) & 0x3F
        b5 = (b >> 3) & 0x1F
        rgb565 = (r5 << 11) | (g6 << 5) | b5
        rgb_data.append(rgb565 & 0xFF)
        rgb_data.append((rgb565 >> 8) & 0xFF)
        alpha_data.append(a)

data = rgb_data + alpha_data

# Write .c file
outpath_c = os.path.join(src_dir, 'button_4_106x40.c')
with open(outpath_c, 'w') as f:
    f.write('#include <lvgl.h>\n\n')
    f.write('#ifndef LV_ATTRIBUTE_MEM_ALIGN\n')
    f.write('#define LV_ATTRIBUTE_MEM_ALIGN\n')
    f.write('#endif\n\n')
    f.write('const LV_ATTRIBUTE_MEM_ALIGN uint8_t button_4_106x40_map[] = {\n')
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_vals = ', '.join(f'0x{b:02x}' for b in chunk)
        comma = ',' if i + 16 < len(data) else ''
        f.write(f'  {hex_vals}{comma}\n')
    f.write('};\n\n')
    f.write('const lv_img_dsc_t button_4_106x40 = {\n')
    f.write('  .header.cf = LV_IMG_CF_RGB565A8,\n')
    f.write('  .header.always_zero = 0,\n')
    f.write('  .header.reserved = 0,\n')
    f.write(f'  .header.w = {DST_W},\n')
    f.write(f'  .header.h = {DST_H},\n')
    f.write(f'  .data_size = {len(data)},\n')
    f.write('  .data = button_4_106x40_map,\n')
    f.write('};\n')

# Write .h file
outpath_h = os.path.join(src_dir, 'button_4_106x40.h')
with open(outpath_h, 'w') as f:
    f.write('#ifndef BUTTON_4_106X40_H\n')
    f.write('#define BUTTON_4_106X40_H\n\n')
    f.write('#ifdef __cplusplus\n')
    f.write('extern "C" {\n')
    f.write('#endif\n\n')
    f.write('#include <lvgl.h>\n\n')
    f.write('extern const lv_img_dsc_t button_4_106x40;\n\n')
    f.write('#ifdef __cplusplus\n')
    f.write('}\n')
    f.write('#endif\n\n')
    f.write('#endif /* BUTTON_4_106X40_H */\n')

print(f"Written {outpath_c}: {DST_W}x{DST_H}, {len(data)} bytes")
print(f"Written {outpath_h}")
