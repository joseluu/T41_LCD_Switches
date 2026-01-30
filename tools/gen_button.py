"""Generate a translucent plastic push button bitmap for LVGL v8.
Output: src/button_4_106x40.c and src/button_4_106x40.h (RGB565A8 format)
"""
import struct
import math

W = 106
H = 40
CORNER_R = 8  # rounded corner radius

def clamp(v, lo=0, hi=255):
    return max(lo, min(hi, int(v)))

def make_button():
    """Generate RGBA pixels for a translucent plastic button."""
    pixels = []  # list of (r, g, b, a) tuples

    for y in range(H):
        for x in range(W):
            # Normalized coordinates
            nx = x / (W - 1)  # 0..1
            ny = y / (H - 1)  # 0..1

            # Rounded rectangle mask (alpha for corners)
            # Distance from nearest edge for corner rounding
            dx = max(0, CORNER_R - x, x - (W - 1 - CORNER_R))
            dy = max(0, CORNER_R - y, y - (H - 1 - CORNER_R))
            dist = math.sqrt(dx * dx + dy * dy)
            if dist > CORNER_R:
                # Outside rounded corner
                pixels.append((0, 0, 0, 0))
                continue

            # Edge softness (anti-aliasing on corners)
            edge_alpha = clamp(255 * (CORNER_R - dist + 0.5))

            # --- Plastic translucent look ---
            # Base: mostly transparent white/light gray
            # Top highlight: bright, less transparent
            # Bottom shadow: darker, more transparent
            # Side gradients for 3D dome effect

            # Vertical gradient: top bright -> middle transparent -> bottom dark
            if ny < 0.35:
                # Top highlight band
                t = ny / 0.35
                r = clamp(255 - t * 30)
                g = clamp(255 - t * 30)
                b = clamp(255 - t * 20)
                a = clamp(200 - t * 120)  # fairly visible at top, fading
            elif ny < 0.65:
                # Middle: very transparent (background shows through)
                t = (ny - 0.35) / 0.30
                r = clamp(220 - t * 20)
                g = clamp(220 - t * 20)
                b = clamp(215 - t * 15)
                a = clamp(40 + t * 20)  # low alpha = translucent
            else:
                # Bottom shadow
                t = (ny - 0.65) / 0.35
                r = clamp(180 - t * 80)
                g = clamp(180 - t * 80)
                b = clamp(180 - t * 60)
                a = clamp(60 + t * 100)  # getting more opaque toward bottom

            # Horizontal edge darkening (subtle 3D)
            edge_x = min(nx, 1.0 - nx) * 2.0  # 0 at edges, 1 at center
            edge_factor = 0.7 + 0.3 * edge_x
            r = clamp(r * edge_factor)
            g = clamp(g * edge_factor)
            b = clamp(b * edge_factor)

            # Top specular highlight (narrow bright strip)
            if ny < 0.15:
                spec = (0.15 - ny) / 0.15
                # Only in the middle 60% horizontally
                hspec = max(0, 1.0 - abs(nx - 0.5) / 0.3)
                spec *= hspec
                r = clamp(r + spec * 60)
                g = clamp(g + spec * 60)
                b = clamp(b + spec * 70)
                a = clamp(a + spec * 80)

            # Bottom edge highlight (thin bright line simulating reflection)
            if ny > 0.90:
                t = (ny - 0.90) / 0.10
                r = clamp(r + t * 30)
                g = clamp(g + t * 30)
                b = clamp(b + t * 35)

            # Apply corner anti-aliasing
            final_a = clamp(a * edge_alpha / 255)

            pixels.append((r, g, b, final_a))

    return pixels


def rgb888_to_rgb565(r, g, b):
    """Convert 8-bit RGB to RGB565 (16-bit, little-endian)."""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5


def write_c_file(pixels, outpath_c, outpath_h):
    """Write LVGL v8 C source with LV_IMG_CF_RGB565A8 format.
    Layout: all RGB565 pixels first (W*H*2 bytes), then all alpha bytes (W*H bytes)."""
    rgb_data = bytearray()
    alpha_data = bytearray()
    for (r, g, b, a) in pixels:
        rgb565 = rgb888_to_rgb565(r, g, b)
        # Little-endian 16-bit
        rgb_data.append(rgb565 & 0xFF)
        rgb_data.append((rgb565 >> 8) & 0xFF)
        alpha_data.append(a)

    data = rgb_data + alpha_data
    total_pixels = W * H

    # Write .c file
    with open(outpath_c, 'w') as f:
        f.write('#include <lvgl.h>\n\n')
        f.write('#ifndef LV_ATTRIBUTE_MEM_ALIGN\n')
        f.write('#define LV_ATTRIBUTE_MEM_ALIGN\n')
        f.write('#endif\n\n')
        f.write(f'const LV_ATTRIBUTE_MEM_ALIGN uint8_t button_4_106x40_map[] = {{\n')

        # Write data in rows of 16 bytes
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_vals = ', '.join(f'0x{b:02x}' for b in chunk)
            comma = ',' if i + 16 < len(data) else ''
            f.write(f'  {hex_vals}{comma}\n')

        f.write('};\n\n')
        f.write(f'const lv_img_dsc_t button_4_106x40 = {{\n')
        f.write(f'  .header.cf = LV_IMG_CF_RGB565A8,\n')
        f.write(f'  .header.always_zero = 0,\n')
        f.write(f'  .header.reserved = 0,\n')
        f.write(f'  .header.w = {W},\n')
        f.write(f'  .header.h = {H},\n')
        f.write(f'  .data_size = {len(data)},\n')
        f.write(f'  .data = button_4_106x40_map,\n')
        f.write(f'}};\n')

    # Write .h file
    with open(outpath_h, 'w') as f:
        f.write('#ifndef BUTTON_3_WIP_2_H\n')
        f.write('#define BUTTON_3_WIP_2_H\n\n')
        f.write('#ifdef __cplusplus\n')
        f.write('extern "C" {\n')
        f.write('#endif\n\n')
        f.write('#include <lvgl.h>\n\n')
        f.write('extern const lv_img_dsc_t button_4_106x40;\n\n')
        f.write('#ifdef __cplusplus\n')
        f.write('}\n')
        f.write('#endif\n\n')
        f.write('#endif /* BUTTON_3_WIP_2_H */\n')

    print(f'Generated {outpath_c}: {W}x{H}, {len(data)} bytes ({total_pixels} pixels)')
    print(f'Generated {outpath_h}')


if __name__ == '__main__':
    import os
    script_dir = os.path.dirname(os.path.abspath(__file__))
    src_dir = os.path.join(script_dir, '..', 'src')
    pixels = make_button()
    write_c_file(pixels,
                 os.path.join(src_dir, 'button_4_106x40.c'),
                 os.path.join(src_dir, 'button_4_106x40.h'))
