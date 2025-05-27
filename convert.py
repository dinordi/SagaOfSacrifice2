import json
import re

def get_sort_key(sprite):
    """Extract numeric value from filename for proper sorting"""
    filename = sprite['filename']
    
    # Handle base filename without number
    if filename == 'letters_small':
        return 0
    
    # Extract number from filename like "x-25.png"
    match = re.search(r'letters_small-(\d+)\.png', filename)
    if match:
        return int(match.group(1))
    
    # Fallback for any unexpected filenames
    return 999

# Read the JSON file
with open('SOS/assets/spriteatlas/letters_small.tpsheet', 'r') as f:
    data = json.load(f)

# Sort sprites by filename
data['textures'][0]['sprites'].sort(key=get_sort_key)

# Write back to file with proper formatting
with open('SOS/assets/spriteatlas/letters_small.tpsheet', 'w') as f:
    json.dump(data, f, indent='\t')

print('Sprites sorted successfully!')