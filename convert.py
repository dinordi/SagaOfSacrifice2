import json
import os
import re
import glob

# ────────────────────────────────────────────────────────────────
#  helpers
# ────────────────────────────────────────────────────────────────

IMAGE_EXTS = {'.png', '.jpg', '.jpeg', '.gif', '.bmp', '.tiff', '.webp'}

def strip_image_ext(name: str) -> str:
    """
    If `name` ends with one of IMAGE_EXTS (case-insensitive) return the name
    without the extension, otherwise return it unchanged.
    """
    base, ext = os.path.splitext(name)
    return base if ext.lower() in IMAGE_EXTS else name


def get_sort_key(sprite: dict) -> int:
    """Numeric suffix of a sprite filename → int (default 0)."""
    filename = sprite.get('filename', '')
    match = re.search(r'(\d+)$', os.path.splitext(filename)[0])
    return int(match.group(1)) if match else 0


# ────────────────────────────────────────────────────────────────
#  optional deep-clean of *all* strings (same logic you had)
# ────────────────────────────────────────────────────────────────

def clean_string_value(value: str) -> str:
    if not isinstance(value, str):
        return value

    original = value

    # remove extensions
    for ext in IMAGE_EXTS:
        if value.lower().endswith(ext):
            value = value[:-len(ext)]
            break

    # remove “(1)”, “(2)” … style duplicates
    value = re.sub(r'\(\d+\)', '', value)

    # tidy up double separators / spaces
    value = re.sub(r'--+', '-', value)
    value = re.sub(r'__+', '_', value)
    value = re.sub(r'\s\s+', ' ', value)
    value = value.rstrip('-_ ')

    return value


def clean_json_recursively(obj, path=""):
    """
    Walk every string in the JSON object; apply `clean_string_value`.
    Returns list of human-readable change descriptions.
    """
    changes = []

    if isinstance(obj, dict):
        for k, v in obj.items():
            p = f"{path}.{k}" if path else k
            if isinstance(v, str):
                new = clean_string_value(v)
                if new != v:
                    obj[k] = new
                    changes.append(f"    {p}: '{v}' → '{new}'")
            elif isinstance(v, (dict, list)):
                changes.extend(clean_json_recursively(v, p))

    elif isinstance(obj, list):
        for i, item in enumerate(obj):
            p = f"{path}[{i}]"
            if isinstance(item, str):
                new = clean_string_value(item)
                if new != item:
                    obj[i] = new
                    changes.append(f"    {p}: '{item}' → '{new}'")
            elif isinstance(item, (dict, list)):
                changes.extend(clean_json_recursively(item, p))

    return changes


# ────────────────────────────────────────────────────────────────
#  main file-level routine
# ────────────────────────────────────────────────────────────────

def sort_atlas_file(filepath: str) -> bool:
    """
    Load one atlas (.tpsheet or .json), strip extensions from the
    'image' fields, deep-clean strings, sort textures & sprites,
    and write it back.
    """

    try:
        fn = os.path.basename(filepath)
        print(f"\nProcessing: {fn}")

        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)

        textures = data.get('textures')
        if not textures:
            print("  ERROR: no 'textures' array")
            return False

        # clean & sort each texture
        for tex in textures:
            # 1) tidy the image name
            orig_image = tex.get('image')
            if isinstance(orig_image, str):
                new_image = strip_image_ext(orig_image)
                if new_image != orig_image:
                    tex['image'] = new_image
                    print(f"  • image: {orig_image!r} → {new_image!r}")

            # 2) tidy every sprite filename then sort sprites
            sprites = tex.get('sprites', [])
            for spr in sprites:
                fn_orig = spr.get('filename')
                if isinstance(fn_orig, str):
                    fn_new = strip_image_ext(fn_orig)
                    if fn_new != fn_orig:
                        spr['filename'] = fn_new
            sprites.sort(key=get_sort_key)

        # 3) sort the textures themselves (after images cleaned)
        textures.sort(key=lambda t: t.get('image', ''))

        # 4) (optional) deep-clean every string (kept from your script)
        changes = clean_json_recursively(data)
        if changes:
            print("  Deep-clean changes:")
            for line in changes[:10]:
                print(line)
            if len(changes) > 10:
                print(f"    … and {len(changes)-10} more")

        # 5) write back
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=4, ensure_ascii=False, sort_keys=True)

        print("  ✓ Saved")
        return True

    except Exception as exc:
        print(f"  ERROR: {exc}")
        return False


# ────────────────────────────────────────────────────────────────
#  driver
# ────────────────────────────────────────────────────────────────

def main():
    spriteatlas_dir = 'SOS/assets/spriteatlas'
    if not os.path.isdir(spriteatlas_dir):
        print(f"ERROR: directory {spriteatlas_dir!r} not found.")
        return

    # pick up both .tpsheet *and* .json files
    patterns = ('*.tpsheet', '*.json')
    atlas_files = []
    for pat in patterns:
        atlas_files.extend(glob.glob(os.path.join(spriteatlas_dir, pat)))

    if not atlas_files:
        print(f"ERROR: no atlas files found in {spriteatlas_dir!r}")
        return

    print(f"Found {len(atlas_files)} atlas files to process")
    print("=" * 60)

    ok = 0
    for path in sorted(atlas_files):
        if sort_atlas_file(path):
            ok += 1

    print("\n" + "=" * 60)
    print(f"Summary: {ok}/{len(atlas_files)} files processed successfully")
    if ok != len(atlas_files):
        print(f"WARNING: {len(atlas_files)-ok} file(s) failed.")

if __name__ == "__main__":
    main()
