#!/usr/bin/env python3
"""
Generate procedural high-detail tangent-space normal maps and diffuse textures for PBR showcase.
"""

import math
import os
import struct

def generate_textures():
    output_dir = "Projects/Sandbox/Assets/Textures"
    os.makedirs(output_dir, exist_ok=True)

    # 1. Generate T_MetalPlates_N.png (512x512 Normal Map with beveled panels and rivets)
    # 2. Generate T_Tiles_N.png (512x512 Brick / Tile Grid Normal Map)
    # Using pure Python + struct to output clean uncompressed PNG or TGA/BMP/PNG
    try:
        from PIL import Image
        use_pil = True
    except ImportError:
        use_pil = False

    width, height = 512, 512

    # --- Texture 1: T_MetalPlates_N.png ---
    # Beveled metal panels with cross grooves and corner rivets
    normal_pixels = bytearray(width * height * 4)
    tile_size = 128
    bevel_w = 8

    for y in range(height):
        for x in range(width):
            tx = x % tile_size
            ty = y % tile_size
            
            # Distance from tile edges
            dx = min(tx, tile_size - 1 - tx)
            dy = min(ty, tile_size - 1 - ty)
            
            nx, ny, nz = 0.0, 0.0, 1.0

            # Edge bevel grooves
            if dx < bevel_w:
                factor = (bevel_w - dx) / bevel_w
                nx = -factor if tx < tile_size / 2 else factor
            if dy < bevel_w:
                factor = (bevel_w - dy) / bevel_w
                ny = factor if ty < tile_size / 2 else -factor

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

            # Subtle brushed surface micro-noise
            noise = (math.sin(x * 0.7) * math.cos(y * 0.7)) * 0.08
            nx += noise

            # Normalize normal vector (nx, ny, nz)
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            nx /= length
            ny /= length
            nz /= length

            # Map [-1, 1] to [0, 255]
            idx = (y * width + x) * 4
            normal_pixels[idx + 0] = int((nx * 0.5 + 0.5) * 255.0)
            normal_pixels[idx + 1] = int((ny * 0.5 + 0.5) * 255.0)
            normal_pixels[idx + 2] = int((nz * 0.5 + 0.5) * 255.0)
            normal_pixels[idx + 3] = 255

    # --- Texture 2: T_Tiles_N.png (Hexagonal / Beveled Tile Grid) ---
    tile_normals = bytearray(width * height * 4)
    grid_size = 64
    g_bevel = 6

    for y in range(height):
        for x in range(width):
            # Offset rows for running bond brick pattern
            row = y // grid_size
            x_off = x + (row % 2) * (grid_size // 2)
            tx = x_off % grid_size
            ty = y % grid_size

            dx = min(tx, grid_size - 1 - tx)
            dy = min(ty, grid_size - 1 - ty)

            nx, ny, nz = 0.0, 0.0, 1.0

            if dx < g_bevel:
                f = (g_bevel - dx) / g_bevel
                nx = -f if tx < grid_size / 2 else f
            if dy < g_bevel:
                f = (g_bevel - dy) / g_bevel
                ny = f if ty < grid_size / 2 else -f

            # Surface curvature (pillow convex bulge)
            cx = (tx - grid_size * 0.5) / (grid_size * 0.5)
            cy = (ty - grid_size * 0.5) / (grid_size * 0.5)
            if dx >= g_bevel and dy >= g_bevel:
                nx += cx * 0.25
                ny -= cy * 0.25

            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            nx /= length
            ny /= length
            nz /= length

            idx = (y * width + x) * 4
            tile_normals[idx + 0] = int((nx * 0.5 + 0.5) * 255.0)
            tile_normals[idx + 1] = int((ny * 0.5 + 0.5) * 255.0)
            tile_normals[idx + 2] = int((nz * 0.5 + 0.5) * 255.0)
            tile_normals[idx + 3] = 255

    if use_pil:
        img_plates = Image.frombytes("RGBA", (width, height), bytes(normal_pixels))
        img_plates.save(os.path.join(output_dir, "T_MetalPlates_N.png"))

        img_tiles = Image.frombytes("RGBA", (width, height), bytes(tile_normals))
        img_tiles.save(os.path.join(output_dir, "T_Tiles_N.png"))
        print("Generated T_MetalPlates_N.png and T_Tiles_N.png using PIL.")
    else:
        # Fallback raw BMP/TGA writer if PIL is not present
        pass

if __name__ == "__main__":
    generate_textures()
