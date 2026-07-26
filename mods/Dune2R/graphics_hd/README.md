# Dune2R Enhanced Graphics

Place high-resolution anchored graphics here.

## Animated units

Enhanced unit animations are complete-unit atlases under `units/`. They are
selected by stable game item ID and, optionally, house ID. This avoids changing
shared classic chassis/turret sprites used by other vehicles.

```text
units/
  ordostank/
    unit.ini
    atlases/
      idle/east.png
      movement/east.png
      combat/east.png
```

Each atlas is a row-major grid of transparent PNG frames. Example manifest:

```ini
[Unit]
ItemID=41
HouseID=2

[Render]
BaseWidth=48
BaseHeight=48
Scale=1.21

[Idle.east]
Atlas=atlases/idle/east.png
Columns=8
Rows=6
Frames=41
FrameMs=90
AnchorX=96
AnchorY=96
Loop=true
```

Supported visual states are `Idle`, `Movement`, and `Combat`; supported
directions are `east`, `north_east`, `north`, `north_west`, `west`,
`south_west`, `south`, and `south_east`. Missing states or directions fall
through to the classic renderer. Combat is triggered only by an actual weapon
fire event and remains cosmetic; it does not change simulation or save data.
Complete-tank sprites use the chassis direction while moving and the weapon
direction while stationary or firing, presenting the vehicle as a rigid
Dune II-style unit without changing projectile or targeting mechanics.

When Dune2R is active, its in-game Zoom button cycles three local presentation
views: Action (3x), Tactical (2x), and Strategic (1x). The selected view changes
only rendering and camera coverage; simulation, multiplayer synchronization,
and save data are unaffected. The control is hidden for every other mod. The
mouse wheel changes view while it is over the map; scrollable interface widgets
retain priority and continue consuming wheel input normally.

Package numbered Oathkeeper PNG frames with:

```text
python scripts/package-dune2r-unit.py <dune2/units/unit> <mods/Dune2R/graphics_hd/units/unit> --item-id 41 --house-id 2 --scale 1.21
```

Only the active mod's `graphics_hd/units` directory is scanned. Switching away
from Dune2R clears every enhanced texture and manifest cache.

Current supported layout:

```text
objpics/
  ObjPic_Tank_Base.png
  ObjPic_Tank_Base.ini
  ObjPic_Tank_Gun.png
  ObjPic_Tank_Gun.ini
```

Each PNG is an atlas. For Dune II-style facing sprites, use 8 columns by 1 row
in the same draw-angle order as classic objpics.

Example metadata:

```ini
[Sprite]
Columns=8
Rows=1
AnchorX=256
AnchorY=256

[Render]
BaseWidth=48
BaseHeight=48
Scale=1.18
```

`AnchorX` and `AnchorY` are measured in the source frame. The anchor lands on
the same world point used by classic unit sprites. `BaseWidth` and `BaseHeight`
are zoom-0 screen pixels before `Scale`; zoom levels multiply them by 1, 2, and
3.

Large padded Dune2R renders belong here. Tiny Dune II compact replacements
belong under `graphics_compact/`.
