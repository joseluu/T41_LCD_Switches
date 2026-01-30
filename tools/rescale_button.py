"""Rescale button_3_wip_2.png (116x68) to 106x40 and output as LVGL v8 C array."""
from PIL import Image
import os

script_dir = os.path.dirname(os.path.abspath(__file__))
src_dir = os.path.join(script_dir, '..', 'src')

img = Image.open(os.path.join(src_dir, 'button_3_wip_2.png'))
print(f"Source: {img.size}, mode={img.mode}")

img_resized = img.resize((106, 40), Image.LANCZOS)
print(f"Resized: {img_resized.size}")

W, H = 106, 40
data = bytearray()
for y in range(H):
    for x in range(W):
        r, g, b, a = img_resized.getpixel((x, y))
        r5 = (r >> 3) & 0x1F
        g6 = (g >> 2) & 0x3F
        b5 = (b >> 3) & 0x1F
        rgb565 = (r5 << 11) | (g6 << 5) | b5
        data.append(rgb565 & 0xFF)
        data.append((rgb565 >> 8) & 0xFF)
        data.append(a)

outpath = os.path.join(src_dir, 'button_4_106x40.c')

with open(outpath, 'w') as f:
    f.write('#include <lvgl.h>\n\n')
    f.write('#ifndef LV_ATTRIBUTE_MEM_ALIGN\n')
    f.write('#define LV_ATTRIBUTE_MEM_ALIGN\n')
    f.write('#endif\n\n')
    f.write('#ifndef LV_ATTRIBUTE_IMG_BUTTON_4_106X40\n')
    f.write('#define LV_ATTRIBUTE_IMG_BUTTON_4_106X40\n')
    f.write('#endif\n\n')
    f.write('const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_BUTTON_4_106X40 uint8_t button_4_106x40_map[] = {\n')
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_vals = ', '.join(f'0x{b:02x}' for b in chunk)
        comma = ',' if i + 16 < len(data) else ''
        f.write(f'  {hex_vals}{comma}\n')
    f.write('};\n\n')
    f.write('const lv_img_dsc_t button_4_106x40 = {\n')
    f.write('  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n')
    f.write('  .header.always_zero = 0,\n')
    f.write('  .header.reserved = 0,\n')
    f.write(f'  .header.w = {W},\n')
    f.write(f'  .header.h = {H},\n')
    f.write(f'  .data_size = {len(data)},\n')
    f.write('  .data = button_4_106x40_map,\n')
    f.write('};\n')

print(f"Written {outpath}: {W}x{H}, {len(data)} bytes")
