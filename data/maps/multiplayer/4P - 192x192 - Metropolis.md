# Metropolis

*A Dune City urban-siege scenario for four players on a 192 × 192 map.*

The desert has become a metropolis. Four rival cities have spread from the
corners of the map, laying roads through the sand and building whole districts
behind dense missile-turret defences. The spice fields are spent. Factories,
power stations and the city economy now sustain a war that nobody has managed
to finish.

Take command of an established city and break the stalemate. Protect its power,
keep its neighbourhoods growing, and find a way through the opposing industrial
belts. Destroying a power centre or opening a road corridor can matter more
than another frontal assault on a wall of turrets.

## Setup

Select **Dune City** in the Mods menu, then choose **4P - 192x192 - Metropolis**
from the multiplayer maps. Fill all four slots with humans or AI for the full
scenario. Choose factions and teams in the lobby; the four geographical starts
are assigned by the game's normal slot selection. A four-way battle or two
teams of two suits this map. The normal victory condition is elimination of the
opposition; there is no timer or scripted reinforcement wave.

This is an asymmetric scenario taken from a played game, not a balanced opening
map. The northwest city is smaller and more exposed. The other three occupy
larger districts, with different armies and industrial capacity. Every city
starts with the saved treasury of roughly one million credits, giving players
room to repair, rebuild and choose their own strategy.

| City position | Buildings | Saved starting credits |
| --- | ---: | ---: |
| Northwest | 653 | 999,975 |
| Northeast | 1,232 | 999,999 |
| Southwest | 1,202 | 999,999 |
| Southeast | 1,021 | 999,999 |

The map contains 4,108 standing buildings, including 2,478 residential,
commercial and industrial zones, 928 missile turrets, 39 nuclear plants and
32 construction yards. There are 284 deployed units and 12,677 authored road
tiles. Tech level is 8. With four large cities, this is a heavier scenario than
a normal skirmish.

## Conversion notes

Metropolis comes from Stefan's **stalemate.dls**, saved on 8 September 2026.
Terrain, building and on-map unit positions, ownership by starting slot, damage,
unit facing, concrete, roads and starting credits come from that save. The
original factions were Rebels (northwest), Harkonnen (northeast), Neutral
(southwest) and Ordos (southeast); generic player slots allow lobby faction
selection instead.

A scenario begins a new match. Population and zone density grow again from fresh
zones; upgrades, factory queues, cargo, current orders, fog of war, economic
history and AI state do not transfer. Units begin in Area Guard. Twenty-one
carryalls already outside the map boundary are omitted. Previously unowned
roads and concrete are assigned to their city quadrant. The current engine
also generates road connections around placed buildings; validation observed
258 additional road tiles, while retaining every authored road tile.

Verified by loading the exported INI in the native DuneCity engine with four
occupied player slots: all 36,864 terrain tiles and all 4,108 buildings matched,
all 284 exported units loaded, and no map-placement warnings were emitted.
This checks scenario loading, not competitive balance or a completed network
match.

Source save SHA-256:
`0eaf56b849bfcc7c1c18006e05c4677292905d39e858f6dae8ed2f580b5b0946`

Derived from the save's embedded CC-BY-SA map; the CC-BY-SA designation is
retained. City layout and scenario: Stefan van der Wel.
