#!/usr/bin/env python3
"""
bake_level.py

Usage:
    python3 bake_level.py /path/to/level.json

This script reads a Tiled-style JSON level file (level.json) and produces
a C++ header named LevelData_<levelId>.h alongside the JSON. The header
now contains:
  - constexpr TILE_WIDTH, TILE_HEIGHT, WIDTH, HEIGHT, SIZE
  - static const uint32_t TILE_LAYER_<i>_TILEMAP[SIZE] for every tilelayer
  - constexpr size_t LAYER_COUNT
  - static constexpr const uint32_t* LAYERS[LAYER_COUNT]
  - static constexpr const char* LAYER_NAMES[LAYER_COUNT]
  - struct GidRange and static const GidRange RANGES[]
  - static constexpr const char* TILESET_NAMES[]
  - constexpr PLAYER_START_X, PLAYER_START_Y
  - struct EnemyData and static const EnemyData ENEMIES[], plus ENEMY_COUNT
"""

import json
import os
import sys
import re

def sanitize_identifier(s: str) -> str:
    """
    Replace any non-alphanumeric character with underscore.
    If it starts with a digit, prefix with underscore.
    """
    out = re.sub(r'[^0-9A-Za-z]', '_', s)
    if re.match(r'^[0-9]', out):
        out = '_' + out
    return out

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} /path/to/level.json")
        sys.exit(1)

    json_path = sys.argv[1]
    if not os.path.isfile(json_path):
        print(f"Error: file does not exist: {json_path}")
        sys.exit(1)

    # Load JSON
    with open(json_path, 'r', encoding='utf-8') as f:
        try:
            level = json.load(f)
        except Exception as e:
            print(f"Error: failed to parse JSON: {e}")
            sys.exit(1)

    # 1) Extract "id" → level_id
    if 'id' not in level or not isinstance(level['id'], str):
        print('Error: JSON must contain a string field "id".')
        sys.exit(1)
    level_id = level['id']
    ident = sanitize_identifier(level_id)

    # 2) Extract tileWidth, tileHeight
    if 'tileWidth' not in level or 'tileHeight' not in level:
        print('Error: JSON must contain "tileWidth" and "tileHeight".')
        sys.exit(1)
    tile_width = level['tileWidth']
    tile_height = level['tileHeight']
    if not (isinstance(tile_width, int) and isinstance(tile_height, int)):
        print('Error: "tileWidth" and "tileHeight" must be integers.')
        sys.exit(1)

    # 3) Collect all "tilelayer" entries in level["layers"]
    if 'layers' not in level or not isinstance(level['layers'], list):
        print('Error: JSON must contain an array "layers".')
        sys.exit(1)

    tilelayers = []
    for layer in level['layers']:
        if isinstance(layer, dict) and layer.get('type') == 'tilelayer':
            # Check required fields
            if ('width' not in layer or 'height' not in layer or 'data' not in layer):
                print('Error: each tilelayer must have "width", "height", and "data".')
                sys.exit(1)
            W = layer['width']
            H = layer['height']
            data_arr = layer['data']
            if not (isinstance(W, int) and isinstance(H, int) and isinstance(data_arr, list)):
                print('Error: tilelayer.width/height must be ints and data must be a list.')
                sys.exit(1)
            if W * H != len(data_arr):
                print(f'Error: tilelayer.data length ({len(data_arr)}) != width*height ({W*H}).')
                sys.exit(1)

            # Remember layer name (or fallback)
            layer_name = layer.get('name')
            if not isinstance(layer_name, str) or layer_name.strip() == '':
                layer_name = f"Layer_{len(tilelayers)+1}"
            tilelayers.append({
                'name': layer_name,
                'width': W,
                'height': H,
                'data': data_arr
            })

    if len(tilelayers) == 0:
        print('Error: no tilelayer found in "layers".')
        sys.exit(1)

    # Require every tilelayer to have the SAME width & height
    W0 = tilelayers[0]['width']
    H0 = tilelayers[0]['height']
    for tl in tilelayers:
        if tl['width'] != W0 or tl['height'] != H0:
            print('Error: all tilelayers must share the same width/height.')
            sys.exit(1)

    WIDTH = W0
    HEIGHT = H0
    SIZE = WIDTH * HEIGHT

    # 4) Read tilesets[]
    if 'tilesets' not in level or not isinstance(level['tilesets'], list):
        print('Error: JSON must contain an array "tilesets".')
        sys.exit(1)

    tilesets = []
    for ts in level['tilesets']:
        if not isinstance(ts, dict) or 'firstgid' not in ts:
            print('Error: each tileset must be an object with at least "firstgid".')
            sys.exit(1)
        firstgid = ts['firstgid']
        if not isinstance(firstgid, int):
            print('Error: firstgid must be an integer.')
            sys.exit(1)

        # Determine name: prefer "name", else derive from "source"
        if 'name' in ts and isinstance(ts['name'], str):
            name = ts['name']
        elif 'source' in ts and isinstance(ts['source'], str):
            name = os.path.splitext(os.path.basename(ts['source']))[0]
        else:
            print('Error: tileset missing both "name" and "source".')
            sys.exit(1)

        tilesets.append((firstgid, name))

    # Sort tilesets by firstgid ascending
    tilesets.sort(key=lambda x: x[0])

    # Build GidRange list: for each tileset i, lastgid = next.firstgid - 1 (or UINT32_MAX)
    ranges = []
    for i, (firstgid, name) in enumerate(tilesets):
        if i + 1 < len(tilesets):
            lastgid = tilesets[i+1][0] - 1
        else:
            lastgid = 0xFFFFFFFF
        ranges.append((firstgid, lastgid, i))  # (first, last, tilesetIndex)

    # 5) Extract playerStart (if present)
    player_start_x = None
    player_start_y = None
    if 'playerStart' in level:
        ps = level['playerStart']
        if isinstance(ps, dict) and 'x' in ps and 'y' in ps:
            player_start_x = ps['x']
            player_start_y = ps['y']
            if not (isinstance(player_start_x, int) and isinstance(player_start_y, int)):
                print('Error: playerStart.x and playerStart.y must be integers.')
                sys.exit(1)

    # 6) Extract enemies[] (if present)
    enemies = []
    if 'enemies' in level:
        if not isinstance(level['enemies'], list):
            print('Error: "enemies" must be an array if present.')
            sys.exit(1)
        for e in level['enemies']:
            if not isinstance(e, dict):
                print('Error: each enemy entry must be an object.')
                sys.exit(1)
            required = ['id', 'type', 'spriteId', 'x', 'y', 'hp', 'speed']
            for key in required:
                if key not in e:
                    print(f'Error: enemy is missing "{key}".')
                    sys.exit(1)
            eid = e['id']
            etype = e['type']
            sprite_id = e['spriteId']
            ex = e['x']
            ey = e['y']
            ehp = e['hp']
            esp = e['speed']
            if not (isinstance(eid, str) and isinstance(etype, str) and
                    isinstance(sprite_id, int) and isinstance(ex, int) and isinstance(ey, int) and
                    isinstance(ehp, int) and isinstance(esp, int)):
                print('Error: enemy fields have incorrect types.')
                sys.exit(1)
            enemies.append({
                'id': eid,
                'type': etype,
                'spriteId': sprite_id,
                'x': ex,
                'y': ey,
                'hp': ehp,
                'speed': esp
            })

    # 7) Prepare output header file path
    base_dir = os.path.dirname(json_path)
    header_name = f"LevelData_{ident}.h"
    header_path = os.path.join(base_dir, header_name)

    # 8) Write header
    with open(header_path, 'w', encoding='utf-8') as out:
        out.write("// -----------------------------------------------------------------------------\n")
        out.write(f"// Auto-generated from {os.path.basename(json_path)} by bake_level.py\n")
        out.write("// -----------------------------------------------------------------------------\n\n")
        out.write("#pragma once\n\n")
        out.write("#include <cstdint>\n\n")

        # 8.1) LEVEL ID comment
        out.write(f"// ===== Level \"{level_id}\" =====\n\n")

        # 8.2) Constants: tile dimensions & grid size
        out.write("// Dimensions & tile size\n")
        out.write(f"constexpr uint16_t {ident}_TILE_WIDTH  = {tile_width};\n")
        out.write(f"constexpr uint16_t {ident}_TILE_HEIGHT = {tile_height};\n")
        out.write(f"constexpr uint16_t {ident}_WIDTH       = {WIDTH};\n")
        out.write(f"constexpr uint16_t {ident}_HEIGHT      = {HEIGHT};\n")
        out.write(f"constexpr size_t   {ident}_SIZE        = {SIZE}u;\n\n")

        # 8.3) PLAYER START (if available)
        if player_start_x is not None and player_start_y is not None:
            out.write("// Player start position (world coordinates)\n")
            out.write(f"constexpr uint32_t {ident}_PLAYER_START_X = {player_start_x}u;\n")
            out.write(f"constexpr uint32_t {ident}_PLAYER_START_Y = {player_start_y}u;\n\n")

        #
        # 8.4) One TILEMAP array per tilelayer
        #
        for idx, tl in enumerate(tilelayers, start=1):
            layer_data = tl['data']
            layer_ident = f"{ident}_TILE_LAYER_{idx}_TILEMAP"
            out.write(f"// Layer {idx}: \"{tl['name']}\" (raw GID map)\n")
            out.write(f"static const uint32_t {layer_ident}[{ident}_SIZE] = {{\n")
            for row in range(HEIGHT):
                out.write("    ")
                for col in range(WIDTH):
                    index = row * WIDTH + col
                    gid = layer_data[index]
                    out.write(str(gid))
                    if index < (WIDTH * HEIGHT - 1):
                        out.write(", ")
                out.write("\n")
            out.write("};\n\n")

        # 8.5) LAYER_COUNT and array of pointers LAYERS[]
        out.write("// Number of tile-layers\n")
        out.write(f"constexpr size_t {ident}_LAYER_COUNT = {len(tilelayers)};\n\n")

        out.write("// Pointers to each layer’s raw GID buffer\n")
        out.write(f"static constexpr const uint32_t* {ident}_LAYERS[{ident}_LAYER_COUNT] = {{\n")
        for idx in range(1, len(tilelayers) + 1):
            out.write(f"    {ident}_TILE_LAYER_{idx}_TILEMAP,\n")
        out.write("};\n\n")

        # 8.6) Optional: layer names
        out.write("// Human-readable names for each layer (by index)\n")
        out.write(f"static constexpr const char* {ident}_LAYER_NAMES[{ident}_LAYER_COUNT] = {{\n")
        for tl in tilelayers:
            safe = tl['name'].replace('"', r'\"')
            out.write(f"    \"{safe}\",\n")
        out.write("};\n\n")

        #
        # 8.7) GidRange struct and RANGES array
        #
        out.write("// GID→tilesetIndex mapping\n")
        out.write("struct GidRange {\n")
        out.write("    uint32_t firstGid;\n")
        out.write("    uint32_t lastGid;\n")
        out.write("    uint8_t  tilesetIndex;\n")
        out.write("};\n\n")

        out.write(f"static const GidRange {ident}_RANGES[] = {{\n")
        for firstgid, lastgid, ts_idx in ranges:
            out.write(f"    {{ {firstgid}u, {lastgid}u, (uint8_t){ts_idx} }},\n")
        out.write("};\n")
        out.write(f"static constexpr size_t {ident}_RANGE_COUNT = {len(ranges)};\n\n")

        #
        # 8.8) TILESET_NAMES array
        #
        out.write("// Tileset names (by tilesetIndex)\n")
        out.write(f"static constexpr const char* {ident}_TILESET_NAMES[] = {{\n")
        for _, name in tilesets:
            escaped = name.replace('"', r'\"')
            out.write(f"    \"{escaped}\",\n")
        out.write("};\n\n")

        #
        # 8.9) ENEMIES (if any)
        #
        if enemies:
            out.write("// Enemy definitions\n")
            out.write("struct EnemyData {\n")
            out.write("    const char* id;\n")
            out.write("    const char* type;\n")
            out.write("    uint16_t     spriteId;\n")
            out.write("    uint32_t     x;\n")
            out.write("    uint32_t     y;\n")
            out.write("    uint16_t     hp;\n")
            out.write("    uint16_t     speed;\n")
            out.write("};\n\n")

            out.write(f"static const EnemyData {ident}_ENEMIES[] = {{\n")
            for e in enemies:
                e_id     = e['id'].replace('"', r'\"')
                e_type   = e['type'].replace('"', r'\"')
                e_sprite = e['spriteId']
                e_x      = e['x']
                e_y      = e['y']
                e_hp     = e['hp']
                e_speed  = e['speed']
                out.write(
                    f'    {{ "{e_id}", "{e_type}", (uint16_t){e_sprite}, '
                    f'{e_x}u, {e_y}u, (uint16_t){e_hp}, (uint16_t){e_speed} }},\n'
                )
            out.write("};\n")
            out.write(f"constexpr size_t {ident}_ENEMY_COUNT = {len(enemies)};\n\n")

        # 8.10) End comment
        out.write(f"// End of LevelData_{ident}.h\n")

    print(f"Header written to: {header_path}")

if __name__ == "__main__":
    main()
