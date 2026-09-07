#!/usr/bin/env python3
from __future__ import annotations

import math
import struct
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAPS_DIR = ROOT / 'maps'

I16 = struct.Struct('<h')
I32 = struct.Struct('<i')
VERTEX = struct.Struct('<4fi2f')  # X,Y,Z,rhw,color,tu,tv = 28 bytes
VERTEX_HIT = struct.Struct('<3f')
PROP = struct.Struct('<hhii5fiii')  # 44 bytes: Boolean(2), Integer(2), 10x4-byte fields
COLLIDER = struct.Struct('<ifff')
SPAWN = struct.Struct('<iiii')
WAYPOINT = struct.Struct('<iiii7B5Bi20i')  # 112 bytes


@dataclass
class ParsedMap:
    path: Path
    file_size: int
    version: int
    map_name: str
    texture_name: str
    background1: int
    background2: int
    start_jet: int
    grenade_packs: int
    medikits: int
    weather: int
    steps: int
    map_random_id: int
    poly_count: int
    scenery_count: int
    scenery_element_count: int
    collider_count: int
    spawn_count: int
    waypoint_count: int
    sectors_division: int
    sector_num: int
    sector_total_refs: int
    sector_max_count: int
    sector_nonempty_cells: int
    sector_type3_refs: int
    bbox: tuple[float, float, float, float]
    poly_types: Counter[int]
    startjet_byte_polycount: int
    startjet_byte_candidate_ok: bool
    prop_active_values: Counter[int]
    collider_active_values: Counter[int]
    spawn_active_values: Counter[int]
    waypoint_active_values: Counter[int]
    waypoint_flag_values: dict[str, Counter[int]]
    waypoint_connection_total: int
    waypoint_max_connections: int
    waypoint_bad_connections: int
    waypoint_nonzero_crap: int
    trailer_present: bool
    trailing_zeros: tuple[int, int, int, int] | None
    trailing_bytes: int
    trailing_nonzero: bool
    name_padding_nonzero: bool
    texture_padding_nonzero: bool
    scenery_name_padding_nonzero: int
    anomalies: list[str] = field(default_factory=list)


class ParseError(Exception):
    pass



def read_struct(data: bytes, offset: int, st: struct.Struct, label: str):
    end = offset + st.size
    if end > len(data):
        raise ParseError(f'{label}: need {st.size} bytes at offset {offset}, file ends at {len(data)}')
    return st.unpack_from(data, offset), end



def read_i16(data: bytes, offset: int, label: str):
    (value,), offset = read_struct(data, offset, I16, label)
    return value, offset



def read_i32(data: bytes, offset: int, label: str):
    (value,), offset = read_struct(data, offset, I32, label)
    return value, offset



def read_fixed_pascal(data: bytes, offset: int, size: int, label: str, anomalies: list[str]):
    end = offset + size
    if end > len(data):
        raise ParseError(f'{label}: need {size} bytes at offset {offset}, file ends at {len(data)}')
    raw = data[offset:end]
    strlen = raw[0]
    if strlen > size - 1:
        anomalies.append(f'{label}: length byte {strlen} exceeds capacity {size - 1}')
        strlen = size - 1
    text = raw[1:1 + strlen].decode('latin1', errors='replace')
    padding_nonzero = any(raw[1 + strlen:])
    return text, padding_nonzero, end



def format_counter(counter: Counter[int]) -> str:
    if not counter:
        return '{}'
    return '{' + ', '.join(f'{k}:{counter[k]}' for k in sorted(counter)) + '}'



def parse_map(path: Path) -> ParsedMap:
    data = path.read_bytes()
    anomalies: list[str] = []
    offset = 0

    version, offset = read_i32(data, offset, 'version')
    map_name, name_padding_nonzero, offset = read_fixed_pascal(data, offset, 39, 'mapName', anomalies)
    texture_name, texture_padding_nonzero, offset = read_fixed_pascal(data, offset, 25, 'textureName', anomalies)
    background1, offset = read_i32(data, offset, 'backgroundColor1')
    background2, offset = read_i32(data, offset, 'backgroundColor2')
    start_jet, offset = read_i32(data, offset, 'StartJet')
    if offset + 4 > len(data):
        raise ParseError('missing TOptions byte fields after StartJet')
    grenade_packs = data[offset]
    medikits = data[offset + 1]
    weather = data[offset + 2]
    steps = data[offset + 3]
    offset += 4
    map_random_id, offset = read_i32(data, offset, 'MapRandomID')

    try:
        startjet_byte_polycount = I32.unpack_from(data, 85)[0]
    except struct.error:
        startjet_byte_polycount = -2**31
    startjet_byte_candidate_ok = 0 <= startjet_byte_polycount <= max(1, len(data) // 121)

    poly_count, offset = read_i32(data, offset, 'polygonCount')
    if poly_count < 0:
        raise ParseError(f'negative polygon count {poly_count}')

    min_x = math.inf
    min_y = math.inf
    max_x = -math.inf
    max_y = -math.inf
    poly_types: Counter[int] = Counter()
    poly_types_by_index = [None]

    for poly_index in range(1, poly_count + 1):
        for vertex_index in range(1, 4):
            (x, y, z, rhw, color, tu, tv), offset = read_struct(data, offset, VERTEX, f'poly {poly_index} vertex {vertex_index}')
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)
            if not math.isfinite(x) or not math.isfinite(y):
                anomalies.append(f'polygon {poly_index}: non-finite vertex {vertex_index} coordinate')
            if not math.isfinite(z) or not math.isfinite(rhw) or not math.isfinite(tu) or not math.isfinite(tv):
                anomalies.append(f'polygon {poly_index}: non-finite vertex {vertex_index} attribute')
        for hit_index in range(1, 4):
            (hx, hy, hz), offset = read_struct(data, offset, VERTEX_HIT, f'poly {poly_index} perp {hit_index}')
            if not math.isfinite(hx) or not math.isfinite(hy) or not math.isfinite(hz):
                anomalies.append(f'polygon {poly_index}: non-finite perp {hit_index}')
        if offset >= len(data):
            raise ParseError(f'polygon {poly_index}: missing polyType byte')
        poly_type = data[offset]
        offset += 1
        poly_types[poly_type] += 1
        poly_types_by_index.append(poly_type)

    if poly_count == 0:
        min_x = min_y = max_x = max_y = 0.0

    sectors_division, offset = read_i32(data, offset, 'sectorsDivision')
    sector_num, offset = read_i32(data, offset, 'SECTOR_NUM')
    if sectors_division <= 0:
        anomalies.append(f'sectorsDivision={sectors_division} is not positive')
    if sector_num != 25:
        anomalies.append(f'SECTOR_NUM={sector_num}, expected 25')

    sector_total_refs = 0
    sector_max_count = 0
    sector_nonempty_cells = 0
    sector_type3_refs = 0
    expected_cells = (2 * sector_num + 1) ** 2 if sector_num >= 0 else 0
    for cell_index in range(expected_cells):
        count, offset = read_i16(data, offset, f'sector cell {cell_index} count')
        if count < 0:
            anomalies.append(f'sector cell {cell_index}: negative polygon count {count}')
            count = 0
        sector_max_count = max(sector_max_count, count)
        if count > 0:
            sector_nonempty_cells += 1
        if count > 256:
            anomalies.append(f'sector cell {cell_index}: count {count} exceeds writer clamp 256')
        sector_total_refs += count
        for ref_index in range(count):
            poly_ref, offset = read_i16(data, offset, f'sector cell {cell_index} poly {ref_index + 1}')
            if not 1 <= poly_ref <= poly_count:
                anomalies.append(f'sector cell {cell_index}: polygon index {poly_ref} out of range 1..{poly_count}')
            elif poly_types_by_index[poly_ref] == 3:
                sector_type3_refs += 1

    scenery_count, offset = read_i32(data, offset, 'sceneryCount')
    if scenery_count < 0:
        raise ParseError(f'negative scenery count {scenery_count}')
    prop_active_values: Counter[int] = Counter()
    for i in range(1, scenery_count + 1):
        values, offset = read_struct(data, offset, PROP, f'prop {i}')
        active, style, width, height, x, y, rotation, scale_x, scale_y, alpha, color, level = values
        prop_active_values[active] += 1
        if style < 1:
            anomalies.append(f'prop {i}: style={style} < 1')
        if width < 0 or height < 0:
            anomalies.append(f'prop {i}: negative size {width}x{height}')
        if not all(math.isfinite(v) for v in (x, y, rotation, scale_x, scale_y)):
            anomalies.append(f'prop {i}: non-finite float field')

    scenery_element_count, offset = read_i32(data, offset, 'sceneryElementCount')
    if scenery_element_count < 0:
        raise ParseError(f'negative scenery element count {scenery_element_count}')
    scenery_name_padding_nonzero = 0
    for i in range(1, scenery_element_count + 1):
        _, padding_nonzero, offset = read_fixed_pascal(data, offset, 51, f'sceneryName[{i}]', anomalies)
        if padding_nonzero:
            scenery_name_padding_nonzero += 1
        _, offset = read_i32(data, offset, f'sceneryDate[{i}]')

    collider_count, offset = read_i32(data, offset, 'colliderCount')
    if collider_count < 0:
        raise ParseError(f'negative collider count {collider_count}')
    collider_active_values: Counter[int] = Counter()
    for i in range(1, collider_count + 1):
        (active, x, y, radius), offset = read_struct(data, offset, COLLIDER, f'collider {i}')
        collider_active_values[active] += 1
        if radius < 0:
            anomalies.append(f'collider {i}: negative radius {radius}')
        if not all(math.isfinite(v) for v in (x, y, radius)):
            anomalies.append(f'collider {i}: non-finite float field')

    spawn_count, offset = read_i32(data, offset, 'spawnCount')
    if spawn_count < 0:
        raise ParseError(f'negative spawn count {spawn_count}')
    spawn_active_values: Counter[int] = Counter()
    for i in range(1, spawn_count + 1):
        (active, x, y, team), offset = read_struct(data, offset, SPAWN, f'spawn {i}')
        spawn_active_values[active] += 1
        if team < 0 or team > 31:
            anomalies.append(f'spawn {i}: team {team} outside editor clamp 0..31')

    waypoint_count, offset = read_i32(data, offset, 'waypointCount')
    if waypoint_count < 0:
        raise ParseError(f'negative waypoint count {waypoint_count}')
    waypoint_active_values: Counter[int] = Counter()
    waypoint_flag_values = {k: Counter() for k in ('Left', 'Right', 'Up', 'Down', 'm2', 'pathNum', 'special')}
    waypoint_connection_total = 0
    waypoint_max_connections = 0
    waypoint_bad_connections = 0
    waypoint_nonzero_crap = 0
    for i in range(1, waypoint_count + 1):
        values, offset = read_struct(data, offset, WAYPOINT, f'waypoint {i}')
        active, waypoint_id, x, y = values[:4]
        left, right, up, down, m2, path_num, special = values[4:11]
        crap = values[11:16]
        connections_num = values[16]
        connections = values[17:37]

        waypoint_active_values[active] += 1
        waypoint_flag_values['Left'][left] += 1
        waypoint_flag_values['Right'][right] += 1
        waypoint_flag_values['Up'][up] += 1
        waypoint_flag_values['Down'][down] += 1
        waypoint_flag_values['m2'][m2] += 1
        waypoint_flag_values['pathNum'][path_num] += 1
        waypoint_flag_values['special'][special] += 1

        if waypoint_id != i:
            anomalies.append(f'waypoint {i}: id={waypoint_id}, expected {i}')
        for flag_name, flag_value in [('Left', left), ('Right', right), ('Up', up), ('Down', down), ('m2', m2)]:
            if flag_value not in (0, 1):
                anomalies.append(f'waypoint {i}: {flag_name} flag is {flag_value}, expected 0 or 1')
        if connections_num < 0 or connections_num > 20:
            anomalies.append(f'waypoint {i}: connectionsNum={connections_num} outside 0..20')
            usable = max(0, min(connections_num, 20))
        else:
            usable = connections_num
        waypoint_connection_total += usable
        waypoint_max_connections = max(waypoint_max_connections, usable)
        if any(crap):
            waypoint_nonzero_crap += 1
        for j, target in enumerate(connections[:usable], start=1):
            if not 1 <= target <= waypoint_count:
                waypoint_bad_connections += 1
                anomalies.append(f'waypoint {i}: connection {j} -> {target} out of range 1..{waypoint_count}')

    trailer_present = False
    trailing_zeros = None
    if map_random_id < 0:
        anomalies.append('MapRandomID < 0: extension section present; this analyzer does not decode lights/sketches here')
    else:
        if len(data) - offset >= 8 and data[offset:offset + 8] == b'\x00' * 8:
            trailer_present = True
            trailing_zeros = (0, 0, 0, 0)
            offset += 8
        else:
            anomalies.append('compiled trailer missing (no 4x Integer zero after waypoints)')

    trailing_bytes = len(data) - offset
    trailing_nonzero = any(data[offset:]) if trailing_bytes else False
    if trailing_bytes:
        kind = 'contains non-zero bytes' if trailing_nonzero else 'all zero'
        anomalies.append(f'{trailing_bytes} trailing bytes remain after parse ({kind})')

    return ParsedMap(
        path=path,
        file_size=len(data),
        version=version,
        map_name=map_name,
        texture_name=texture_name,
        background1=background1,
        background2=background2,
        start_jet=start_jet,
        grenade_packs=grenade_packs,
        medikits=medikits,
        weather=weather,
        steps=steps,
        map_random_id=map_random_id,
        poly_count=poly_count,
        scenery_count=scenery_count,
        scenery_element_count=scenery_element_count,
        collider_count=collider_count,
        spawn_count=spawn_count,
        waypoint_count=waypoint_count,
        sectors_division=sectors_division,
        sector_num=sector_num,
        sector_total_refs=sector_total_refs,
        sector_max_count=sector_max_count,
        sector_nonempty_cells=sector_nonempty_cells,
        sector_type3_refs=sector_type3_refs,
        bbox=(min_x, min_y, max_x, max_y),
        poly_types=poly_types,
        startjet_byte_polycount=startjet_byte_polycount,
        startjet_byte_candidate_ok=startjet_byte_candidate_ok,
        prop_active_values=prop_active_values,
        collider_active_values=collider_active_values,
        spawn_active_values=spawn_active_values,
        waypoint_active_values=waypoint_active_values,
        waypoint_flag_values=waypoint_flag_values,
        waypoint_connection_total=waypoint_connection_total,
        waypoint_max_connections=waypoint_max_connections,
        waypoint_bad_connections=waypoint_bad_connections,
        waypoint_nonzero_crap=waypoint_nonzero_crap,
        trailer_present=trailer_present,
        trailing_zeros=trailing_zeros,
        trailing_bytes=trailing_bytes,
        trailing_nonzero=trailing_nonzero,
        name_padding_nonzero=name_padding_nonzero,
        texture_padding_nonzero=texture_padding_nonzero,
        scenery_name_padding_nonzero=scenery_name_padding_nonzero,
        anomalies=anomalies,
    )



def merged_counter(items: list[Counter[int]]) -> Counter[int]:
    total: Counter[int] = Counter()
    for counter in items:
        total.update(counter)
    return total



def main() -> int:
    maps = sorted(MAPS_DIR.glob('*.pms'))
    if not maps:
        print(f'No .pms files found in {MAPS_DIR}')
        return 1

    parsed: list[ParsedMap] = []
    failures: list[tuple[Path, str]] = []

    for path in maps:
        try:
            parsed.append(parse_map(path))
        except Exception as exc:
            failures.append((path, str(exc)))

    print(f'Analyzed {len(parsed)} / {len(maps)} PMS files from {MAPS_DIR}')
    print('')
    print('Per-file summary:')
    for info in parsed:
        min_x, min_y, max_x, max_y = info.bbox
        print(
            f"- {info.path.name}: map='{info.map_name}' tex='{info.texture_name}' polys={info.poly_count} "
            f"scenery={info.scenery_count}/{info.scenery_element_count} spawns={info.spawn_count} "
            f"colliders={info.collider_count} waypoints={info.waypoint_count} rand={info.map_random_id} "
            f"bbox=({min_x:.1f},{min_y:.1f})..({max_x:.1f},{max_y:.1f}) types={format_counter(info.poly_types)}"
        )

    if not parsed:
        print('')
        print('No files parsed successfully.')
        return 1

    print('')
    versions = Counter(info.version for info in parsed)
    start_jets = Counter(info.start_jet for info in parsed)
    map_random_ids = [info.map_random_id for info in parsed]
    compiled = [info for info in parsed if info.map_random_id >= 0]
    extensions = [info for info in parsed if info.map_random_id < 0]
    bbox_all = (
        min(info.bbox[0] for info in parsed),
        min(info.bbox[1] for info in parsed),
        max(info.bbox[2] for info in parsed),
        max(info.bbox[3] for info in parsed),
    )
    global_poly_types: Counter[int] = Counter()
    for info in parsed:
        global_poly_types.update(info.poly_types)

    print('Validation summary:')
    print('- Struct sizes used: TOptions=84, TCustomVertex=28, TPolyHit=36, TMapFile_Polygon=121, TProp=44, TMapFile_Scenery=55, TCollider=16, TSaveSpawnPoint=16, TNewWaypoint=112')
    print(f'- Version values: {format_counter(versions)}')
    print(f'- StartJet values (parsed as Long): {format_counter(start_jets)}')
    print(
        f'- StartJet as Byte layout check: '
        f"{sum(1 for info in parsed if info.startjet_byte_candidate_ok)} plausible alternate polygon counts, "
        f"{sum(1 for info in parsed if not info.startjet_byte_candidate_ok)} implausible"
    )
    print(f'- MapRandomID < 0 maps: {len(extensions)}')
    print(f'- MapRandomID range: min={min(map_random_ids)} max={max(map_random_ids)}')
    print(f'- Global polygon bbox: ({bbox_all[0]:.1f},{bbox_all[1]:.1f})..({bbox_all[2]:.1f},{bbox_all[3]:.1f})')
    print(f'- Global polygon type distribution: {format_counter(global_poly_types)}')
    print(
        f'- sectorsDivision range: min={min(info.sectors_division for info in parsed)} '
        f"max={max(info.sectors_division for info in parsed)}"
    )
    print(
        f'- Sector cell max occupancy: max={max(info.sector_max_count for info in parsed)} '
        f"nonempty cells range={min(info.sector_nonempty_cells for info in parsed)}..{max(info.sector_nonempty_cells for info in parsed)}"
    )
    print(
        f'- Prop active values: {format_counter(merged_counter([info.prop_active_values for info in parsed]))} '
        f"(VB Boolean True serializes as -1)"
    )
    print(f'- Collider active values: {format_counter(merged_counter([info.collider_active_values for info in parsed]))}')
    print(f'- Spawn active values: {format_counter(merged_counter([info.spawn_active_values for info in parsed]))}')
    print(f'- Waypoint active values: {format_counter(merged_counter([info.waypoint_active_values for info in parsed]))}')
    print(
        f'- Waypoint max connections: {max(info.waypoint_max_connections for info in parsed)}; '
        f"bad connection refs: {sum(info.waypoint_bad_connections for info in parsed)}"
    )
    print(f"- Waypoints with non-zero 5-byte pad: {sum(info.waypoint_nonzero_crap for info in parsed)}")
    print(
        f"- Non-zero string padding: mapName={sum(info.name_padding_nonzero for info in parsed)} maps, "
        f"textureName={sum(info.texture_padding_nonzero for info in parsed)} maps, "
        f"scenery entries={sum(info.scenery_name_padding_nonzero for info in parsed)}"
    )
    print(f"- Sector references to polyType 3: {sum(info.sector_type3_refs for info in parsed)}")
    if compiled:
        trailer_present = [info.path.name for info in compiled if info.trailer_present]
        trailer_missing = [info.path.name for info in compiled if not info.trailer_present]
        trailing_extra = [info.path.name for info in compiled if info.trailing_bytes]
        trailing_nonzero = [info.path.name for info in compiled if info.trailing_nonzero]
        print(f'- Compiled trailer present: {len(trailer_present)}/{len(compiled)}; missing: {len(trailer_missing)}')
        print(f'- Extra trailing bytes after logical end: {len(trailing_extra)} maps; non-zero in {len(trailing_nonzero)}')
        if trailer_missing:
            print(f"  Missing trailer: {', '.join(trailer_missing)}")

    if failures:
        print('')
        print('Parse failures:')
        for path, message in failures:
            print(f'- {path.name}: {message}')

    anomaly_maps = [info for info in parsed if info.anomalies]
    print('')
    print(f'Anomalies: {len(anomaly_maps)} maps with issues flagged')
    for info in anomaly_maps:
        print(f'- {info.path.name}:')
        for item in info.anomalies[:20]:
            print(f'    * {item}')
        if len(info.anomalies) > 20:
            print(f'    * ... {len(info.anomalies) - 20} more')

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
