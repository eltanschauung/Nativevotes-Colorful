#!/usr/bin/env python3
"""Build a static catalog from current ratings, mapcycle, BSP entities and analytics.

No database credentials or live queries: supply a JSON analytics export made
using the frontend's qualifying-session query. This tool changes only --output.
"""
import argparse
from collections import Counter, defaultdict
import json
import lzma
from pathlib import Path
import re
import struct


def tokens(text):
    pattern = r'"((?:\\.|[^"\\])*)"|(\{)|(\})|//[^\n]*|/\*[\s\S]*?\*/'
    for m in re.finditer(pattern, text):
        if m.group(1) is not None:
            yield m.group(1).replace('\\"', '"').replace('\\\\', '\\')
        elif m.group(2):
            yield '{'
        elif m.group(3):
            yield '}'


def parse_kv(text):
    seq = iter(tokens(text))

    def section():
        rows = []
        for key in seq:
            if key == '}':
                return rows
            value = next(seq)
            rows.append((key, section() if value == '{' else value))
        raise ValueError('Unterminated KeyValues section')

    root = next(seq)
    if next(seq) != '{':
        raise ValueError('Expected root section')
    rows = section()
    if next(seq, None) is not None:
        raise ValueError('Trailing KeyValues tokens')
    return root, rows


def bsp_entities(path):
    with path.open('rb') as stream:
        header = stream.read(1036)
        if len(header) != 1036 or header[:4] != b'VBSP':
            raise ValueError('Not a Source BSP')
        offset, length = struct.unpack_from('<ii', header, 8)
        if length < 0 or length > 32 * 1024 * 1024:
            raise ValueError('Unreasonable entity lump')
        stream.seek(offset)
        data = stream.read(length)
    if data[:4] == b'LZMA':
        size, packed = struct.unpack_from('<II', data, 4)
        if size > 32 * 1024 * 1024:
            raise ValueError('Unreasonable unpacked entity lump')
        props = data[12:17]
        filters = [{'id': lzma.FILTER_LZMA1, 'dict_size': struct.unpack_from('<I', props, 1)[0],
                    'lc': props[0] % 9, 'lp': (props[0] // 9) % 5, 'pb': props[0] // 45}]
        # Valve raw LZMA lumps do not necessarily contain an EOS marker.
        decoder = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=filters)
        data = decoder.decompress(data[17:17 + packed], max_length=size)
        if len(data) != size:
            raise ValueError('Incomplete entity lump')
    entities = []
    for block in re.findall(rb'\{([^{}]*)\}', data):
        pairs = re.findall(rb'"([^"\r\n]+)"\s*"([^"\r\n]*)"', block)
        entities.append({k.decode(errors='replace').lower(): v.decode(errors='replace') for k, v in pairs})
    return entities


def normalized_name(name):
    return re.sub(r'\.ugc\d+$', '', name.rsplit('/', 1)[-1]).lower()


def classify(name, entities):
    classes = Counter(e.get('classname', '') for e in entities)
    points = [e for e in entities if e.get('classname') == 'team_control_point']
    restricted = any(e.get('classname') in ('team_control_point_master', 'team_control_point_round')
                     and e.get('cpm_restrict_team_cap_win', e.get('cpr_restrict_team_cap_win', '0')) in ('2', '3')
                     for e in entities)
    base = normalized_name(name)
    trains = {e.get('train', '').lower() for e in entities if e.get('classname') == 'team_train_watcher'} - {''}
    cart_capture = any(e.get('classname') == 'trigger_capture_area'
                       and e.get('parentname', '').lower() in trains for e in entities)
    if classes['tf_logic_koth'] and not restricted:
        return 'koth', 'BSP: tf_logic_koth'
    if cart_capture or classes['tf_logic_multiple_escort'] or (trains and base.startswith(('pl_', 'plr_'))):
        return 'payload', 'BSP: cart-linked capture area / payload train watcher'
    if classes['tf_logic_arena']:
        return 'arena', 'BSP: tf_logic_arena'
    if classes['tf_logic_player_destruction']:
        return 'player destruction', 'BSP: player destruction logic'
    if classes['tf_logic_robot_destruction']:
        return 'robot destruction', 'BSP: robot destruction logic'
    if classes['tf_logic_passtime']:
        return 'passtime', 'BSP: passtime logic'
    if base.startswith('tc_'):
        return 'territorial control', 'Known territorial-control family'
    if points:
        if restricted:
            return 'attack/defend', 'BSP: asymmetric cap-win restriction'
        owners = {e.get('point_default_owner', '0') for e in points}
        if len(points) >= 2 and owners in ({'2'}, {'3'}):
            return 'attack/defend', 'BSP: all control points initially owned by one team'
        return 'control points', f'BSP: {len(points)} symmetric control points'
    # Fallback is only used where BSP entities are unavailable. Never infer CP
    # symmetry from cp_ alone: known A/D families precede the generic fallback.
    if re.match(r'cp_(?:gravelpit|dustbowl|steel|gorge(?!.*5gorge)|egypt|mountainlab|mercenarypark|snowplow|sulfur|junction|mossrock|altitude|ambush|hardwood|brew|crasher|degrootkeep|manor|sunken|spookeyridge|stoneyridge)', base):
        return 'attack/defend', 'Known attack/defend family (no BSP CP evidence)'
    families = {'koth_': 'koth', 'pl_': 'payload', 'plr_': 'payload', 'cp_': 'control points',
                'ctf_': 'ctf', 'arena_': 'arena', 'pd_': 'player destruction',
                'rd_': 'robot destruction', 'pass_': 'passtime', 'sd_': 'special delivery',
                'mge_': 'mge', 'vsh_': 'versus saxton hale', 'htf_': 'hold the flag',
                'dom_': 'domination', 'dm_': 'deathmatch', 'trade_': 'trade'}
    for prefix, group in families.items():
        if base.startswith(prefix):
            return group, 'Filename fallback; no usable BSP mode entities'
    return 'other', 'No recognized gameplay mode evidence'


def rank_popular(rows, entries, groups):
    # Frontend defaults: qualifying player-hours DESC, avg_players DESC.
    # Apply the penalty before ranking, not after taking an unpenalized top 50.
    aliases = {normalized_name(name): name for name in entries}
    selected = {}
    for row in rows:
        raw = row['map_name']
        name = aliases.get(normalized_name(raw))
        if not name:
            continue
        factor = 0.70 if groups[name] == 'payload' else 1.0
        score = (round(float(row['player_hours']) * factor, 8),
                 float(row['avg_players']), int(row.get('sessions', 0)))
        if name not in selected or score > selected[name]:
            selected[name] = score
    return sorted(selected, key=lambda name: (-selected[name][0], -selected[name][1],
                                             -selected[name][2], name))[:50], selected


def render(groups, popular):
    lines = ['// Generated from the live mapcycle and existing MapEval ratings.',
             '// Empty value = nomination only; 1 = random/empty; 2 = +1 vote; 3 = +2 votes.',
             '// Legacy negative values are preserved verbatim and treated as level 0.',
             '// Popular: qualifying player-hours, with Payload multiplied by 0.70.',
             '"mapEval"', '{']
    for group, entries in sorted(groups.items(), key=lambda x: (-len(x[1]), x[0])):
        lines += [f'    "{group}"', '    {']
        lines += [f'        "{name}" "{value}"' for name, value in entries]
        lines += ['    }', '']
    lines += ['    "popular maps"', '    {']
    lines += [f'        "{name}" ""' for name in popular]
    return '\n'.join(lines + ['    }', '}', ''])


def build(args):
    _, old = parse_kv(args.existing.read_text())
    entries = {}
    for group, maps in old:
        if group.lower() == 'popular maps':
            continue
        for name, value in maps:
            if not isinstance(value, str):
                raise ValueError('Nested legacy tiers require explicit migration')
            if name in entries and entries[name] != value:
                raise ValueError('Conflicting existing map ratings')
            entries[name] = value
    original = entries.copy()
    cycle = []
    for line in args.mapcycle.read_text().splitlines():
        name = line.split('//', 1)[0].strip()
        if name and name not in cycle:
            cycle.append(name)
            entries.setdefault(name, '')
    index = defaultdict(list)
    for p in args.maps.rglob('*.bsp'):
        index[normalized_name(p.stem)].append(p)
    groups, categories, evidence = defaultdict(list), {}, {}
    for name, value in entries.items():
        direct = args.maps / (name + '.bsp')
        candidates = [direct] if direct.is_file() else index.get(normalized_name(name), [])
        entities = bsp_entities(candidates[0]) if candidates else []
        category, reason = classify(name, entities)
        categories[name] = category
        groups[category].append((name, value))
        evidence[name] = {'category': category, 'reason': reason, 'bsp_found': bool(candidates)}
    rows = json.loads(args.analytics.read_text())
    popular, scores = rank_popular(rows, entries, categories)
    if len(popular) != 50:
        raise ValueError(f'Only {len(popular)} eligible popular maps, expected 50')
    text = render(groups, popular)
    _, check = parse_kv(text)
    rebuilt = {name: value for group, maps in check if group != 'popular maps' for name, value in maps}
    assert all(rebuilt[name] == value for name, value in original.items())
    assert all(name in rebuilt for name in cycle)
    args.output.write_text(text)
    report = {'mapcycle_maps': len(cycle), 'existing_maps': len(original), 'imported_maps': len(entries) - len(original),
              'catalog_maps': len(entries), 'category_counts': {k: len(v) for k, v in sorted(groups.items(), key=lambda x: -len(x[1]))},
              'all_old_values_preserved': True, 'all_cycle_maps_present': True,
              'popular_maps': popular, 'popular_scores': {n: scores[n] for n in popular}, 'classification': evidence}
    args.report.write_text(json.dumps(report, indent=2))
    print(json.dumps({k: v for k, v in report.items() if k not in ('popular_scores', 'classification', 'popular_maps')}, indent=2))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--existing', type=Path, required=True)
    p.add_argument('--mapcycle', type=Path, required=True)
    p.add_argument('--maps', type=Path, required=True)
    p.add_argument('--analytics', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--report', type=Path, required=True)
    build(p.parse_args())


if __name__ == '__main__':
    main()
