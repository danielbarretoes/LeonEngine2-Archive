#!/usr/bin/env python3
"""
Generate procedural high-detail tangent-space normal maps, AO maps, and textures for PBR showcase.
"""

import math
import os

def lerp(a, b, t):
    return a + (b - a) * t

def generate_textures():
    # Paths relative to Engine root when run from repo; or pass LEON_PROJECT Content
    script_dir = os.path.dirname(os.path.abspath(__file__))
    sandbox_root = os.path.abspath(os.path.join(script_dir, ".."))
    output_dir = os.path.join(sandbox_root, "Content", "Textures", "Generated")
    os.makedirs(output_dir, exist_ok=True)

    try:
        from PIL import Image
        use_pil = True
    except ImportError:
        use_pil = False

    width, height = 512, 512

    # ========================================================
    # 1. Metal Plates (Normal Map & AO Map)
    # ========================================================
    normal_pixels = bytearray(width * height * 4)
    ao_pixels = bytearray(width * height * 4)
    tile_size = 128
    bevel_w = 8

    for y in range(height):
        for x in range(width):
            tx = x % tile_size
            ty = y % tile_size
            
            dx = min(tx, tile_size - 1 - tx)
            dy = min(ty, tile_size - 1 - ty)
            
            nx, ny, nz = 0.0, 0.0, 1.0
            ao_val = 1.0

            # Edge bevel grooves
            if dx < bevel_w:
                factor = (bevel_w - dx) / bevel_w
                nx = -factor if tx < tile_size / 2 else factor
                ao_val *= lerp(0.35, 1.0, 1.0 - factor)
            if dy < bevel_w:
                factor = (bevel_w - dy) / bevel_w
                ny = factor if ty < tile_size / 2 else -factor
                ao_val *= lerp(0.35, 1.0, 1.0 - factor)

            # Corner rivets at (24, 24), (104, 24), (24, 104), (104, 104)
            rivet_centers = [(24, 24), (tile_size - 24, 24), (24, tile_size - 24), (tile_size - 24, tile_size - 24)]
            rivet_r = 7.0
            for rx, ry in rivet_centers:
                dist = math.hypot(tx - rx, ty - ry)
                if dist < rivet_r:
                    angle = math.atan2(ty - ry, tx - rx)
                    strength = math.sin((1.0 - dist / rivet_r) * math.pi * 0.5) * 0.85
                    nx += math.cos(angle) * strength
                    ny -= math.sin(angle) * strength
                elif dist < rivet_r + 3.0:
                    crevice = (dist - rivet_r) / 3.0
                    ao_val *= lerp(0.40, 1.0, crevice)

            # Micro-noise
            noise = (math.sin(x * 0.7) * math.cos(y * 0.7)) * 0.05
            nx += noise

            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            nx /= length
            ny /= length
            nz /= length

            idx = (y * width + x) * 4
            normal_pixels[idx + 0] = int((nx * 0.5 + 0.5) * 255.0)
            normal_pixels[idx + 1] = int((ny * 0.5 + 0.5) * 255.0)
            normal_pixels[idx + 2] = int((nz * 0.5 + 0.5) * 255.0)
            normal_pixels[idx + 3] = 255

            ao_byte = int(max(0.0, min(1.0, ao_val)) * 255.0)
            ao_pixels[idx + 0] = ao_byte
            ao_pixels[idx + 1] = ao_byte
            ao_pixels[idx + 2] = ao_byte
            ao_pixels[idx + 3] = 255

    # ========================================================
    # 2. Floor Grid Tiles (Strictly Orthogonal Square Grid & AO)
    # ========================================================
    tile_normals = bytearray(width * height * 4)
    tile_ao = bytearray(width * height * 4)
    grid_size = 128  # 4x4 square tiles across 512x512
    g_bevel = 6

    for y in range(height):
        for x in range(width):
            # Clean orthogonal grid (no diagonal offset / no running bond)
            tx = x % grid_size
            ty = y % grid_size

            dx = min(tx, grid_size - 1 - tx)
            dy = min(ty, grid_size - 1 - ty)

            nx, ny, nz = 0.0, 0.0, 1.0
            ao_val = 1.0

            # Straight vertical and horizontal tile bevels
            if dx < g_bevel:
                f = (g_bevel - dx) / g_bevel
                nx = -f if tx < grid_size / 2 else f
                ao_val *= lerp(0.30, 1.0, 1.0 - f)
            if dy < g_bevel:
                f = (g_bevel - dy) / g_bevel
                ny = f if ty < grid_size / 2 else -f
                ao_val *= lerp(0.30, 1.0, 1.0 - f)

            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            nx /= length
            ny /= length
            nz /= length

            idx = (y * width + x) * 4
            tile_normals[idx + 0] = int((nx * 0.5 + 0.5) * 255.0)
            tile_normals[idx + 1] = int((ny * 0.5 + 0.5) * 255.0)
            tile_normals[idx + 2] = int((nz * 0.5 + 0.5) * 255.0)
            tile_normals[idx + 3] = 255

            ao_byte = int(max(0.0, min(1.0, ao_val)) * 255.0)
            tile_ao[idx + 0] = ao_byte
            tile_ao[idx + 1] = ao_byte
            tile_ao[idx + 2] = ao_byte
            tile_ao[idx + 3] = 255

    if use_pil:
        Image.frombytes("RGBA", (width, height), bytes(normal_pixels)).save(os.path.join(output_dir, "T_MetalPlates_N.png"))
        Image.frombytes("RGBA", (width, height), bytes(ao_pixels)).save(os.path.join(output_dir, "T_MetalPlates_AO.png"))
        Image.frombytes("RGBA", (width, height), bytes(tile_normals)).save(os.path.join(output_dir, "T_Tiles_N.png"))
        Image.frombytes("RGBA", (width, height), bytes(tile_ao)).save(os.path.join(output_dir, "T_Tiles_AO.png"))
        print("Generated orthogonal T_Tiles_N and T_Tiles_AO cleanly.")

if __name__ == "__main__":
    generate_textures()
