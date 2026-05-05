"""Parse battle_moves.h and constants/moves.h to build a move database.

Outputs a Python dict: move_id -> (power, type_id, accuracy, priority, pp, flags).
"""
import re
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]  # pokeemerald root

# 1. Parse MOVE_* constants from include/constants/moves.h
move_ids = {}
with open(ROOT / "include" / "constants" / "moves.h") as f:
    for line in f:
        m = re.match(r"#define\s+(MOVE_\w+)\s+(\d+)", line)
        if m:
            move_ids[m.group(1)] = int(m.group(2))

print(f"Parsed {len(move_ids)} move constants")

# 2. Parse TYPE_* constants from include/constants/pokemon.h
type_ids = {}
with open(ROOT / "include" / "constants" / "pokemon.h") as f:
    for line in f:
        m = re.match(r"#define\s+(TYPE_\w+)\s+(\d+)", line)
        if m:
            type_ids[m.group(1)] = int(m.group(2))

print(f"Parsed {len(type_ids)} type constants")

# 3. Parse EFFECT_* constants
effect_ids = {}
with open(ROOT / "include" / "constants" / "battle_move_effects.h") as f:
    for line in f:
        m = re.match(r"#define\s+(EFFECT_\w+)\s+(\d+)", line)
        if m:
            effect_ids[m.group(1)] = int(m.group(2))

# 4. Parse battle_moves.h
with open(ROOT / "src" / "data" / "battle_moves.h") as f:
    text = f.read()

pattern = r"\[(MOVE_\w+)\]\s*=\s*\{([^}]+)\}"
move_data = {}
for match in re.finditer(pattern, text):
    name = match.group(1)
    body = match.group(2)

    fields = {}
    for line in body.split("\n"):
        fm = re.match(r"\s*\.(\w+)\s*=\s*(.+?),?\s*$", line)
        if fm:
            fields[fm.group(1)] = fm.group(2).strip().rstrip(",")

    mid = move_ids.get(name, -1)
    if mid < 0:
        continue

    # Resolve type
    type_str = fields.get("type", "TYPE_NORMAL")
    type_id = type_ids.get(type_str, 0)

    power = int(fields.get("power", "0"))
    accuracy = int(fields.get("accuracy", "0"))
    priority = int(fields.get("priority", "0"))
    pp = int(fields.get("pp", "0"))

    move_data[mid] = {
        "name": name,
        "power": power,
        "type": type_id,
        "accuracy": accuracy,
        "priority": priority,
        "pp": pp,
    }

print(f"Parsed {len(move_data)} move entries")

# Show some examples
for mid, name in [(1, "POUND"), (57, "SURF"), (85, "THUNDERBOLT"), (14, "SWORDS_DANCE"), (92, "TOXIC")]:
    if mid in move_data:
        d = move_data[mid]
        print(f"  {d['name']}: power={d['power']}, type={d['type']}, acc={d['accuracy']}")

# Save as JSON
out = ROOT / "battle_desktop" / "pokeemerald_battle" / "src" / "pokeemerald_battle" / "move_data.json"
with open(out, "w") as f:
    json.dump(move_data, f, indent=2)
print(f"Saved to {out}")
