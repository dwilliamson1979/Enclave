#!/usr/bin/env python3
"""Locked D1.1C feasibility scoring. Run before inspecting acceptance data.

This analyzer errs toward INCOMPLETE/FAIL for missing evidence. It never
converts an absent/malformed server record into a successful shot.
"""
import argparse
import collections
import datetime as dt
import json
import re
import sys

FIELD = re.compile(r'([A-Za-z][A-Za-z0-9]*)=([^\s()]+)')
# Unreal log timestamp: [2026.10.26-08.12.03:123][...]
TIME = re.compile(r'\[(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d)[:.](\d{3})\]')
SCENARIOS = (1, 2, 3, 4)

def timestamp(line):
    m = TIME.search(line)
    if not m:
        return None
    vals = [int(x) for x in m.groups()]
    return dt.datetime(*vals[:6], microsecond=vals[6]*1000).timestamp()

def fields(line):
    return dict(FIELD.findall(line))

def intval(rec, field):
    try:
        v = int(rec[field])
        if v not in (0, 1):
            raise ValueError
        return v
    except (KeyError, ValueError):
        return None

def analyze(path, band):
    shots = []
    turns = []
    client = collections.defaultdict(list)
    excluded = 0
    with open(path, encoding='utf-8-sig', errors='replace') as file:
        for lineno, line in enumerate(file, 1):
            t = timestamp(line)
            if 'SSRPlayerStrafe LOCAL TURN ' in line:
                turns.append((t, lineno))
            if 'SSRGameplay CLIENT ' in line:
                f = fields(line)
                if f.get('status') == 'far_miss_excluded':
                    excluded += 1
                elif 'key' in f:
                    client[f['key']].append((t, lineno, f))
            if 'SSRGameplay SERVER ' in line:
                f = fields(line)
                f['_line'] = lineno
                f['_time'] = t
                shots.append(f)
    errors = []
    classifications = collections.Counter()
    per_scenario = collections.Counter()
    scored = collections.defaultdict(lambda: {'count': 0, 'correct': 0, 'hits': 0})
    rejection = collections.Counter()
    cover_count = 0
    cover_bad = 0
    reverse_near = 0
    reverse_total = 0
    for f in shots:
        line = f['_line']
        status = f.get('status')
        try:
            scenario = int(f['scenario'])
            category = int(f['category'])
        except (KeyError, ValueError):
            errors.append(f'line {line}: missing/invalid scenario or category')
            continue
        classifications[status] += 1
        if status == 'invalid_claim' or f.get('unresolved') == '1':
            errors.append(f'line {line}: invalid/unresolved claim')
            continue
        primary = intval(f, 'stampFinalHit')
        challenger = intval(f, 'elapsedFinalHit')
        if primary is None or challenger is None:
            errors.append(f'line {line}: final outcome missing/invalid')
            continue
        if scenario == 0 or scenario not in (*SCENARIOS, 5):
            errors.append(f'line {line}: invalid/untagged scenario {scenario}')
            continue
        if category not in (1, 2, 3):
            errors.append(f'line {line}: invalid category {category}')
            continue
        if status in ('rejected_time', 'missing_history', 'query_unavailable') and scenario in (2, 3):
            rejection['unavailable'] += 1
        if scenario in (2, 3):
            rejection['eligible_nonstationary'] += 1
        if scenario == 3 and category in (1, 2):
            reverse_total += 1
            key = f.get('key')
            # Reversal timing must use the client shot timestamp, NOT the
            # server receipt timestamp (which shifts under emulated latency).
            possible = client.get(key, []) if key is not None else []
            # No matching client line, ambiguous key reuse, or no wall-clock
            # in the same log => NOT verified.
            same = [x for x in possible if x[2].get('scenario') == '3' and x[2].get('category') == str(category)]
            if len(same) == 1 and same[0][0] is not None and any(tt is not None and abs(tt-same[0][0]) <= 0.150 for tt, _ in turns):
                reverse_near += 1
            else:
                errors.append(f'line {line}: reversal not independently verified within 150ms of client shot')
        if category == 3:
            continue  # near-miss diagnostic, not accuracy denominator
        if scenario == 5:
            if category == 1 and f.get('stampOccluded') == '1' and f.get('elapsedOccluded') == '1':
                cover_count += 1
                if primary or challenger:
                    cover_bad += 1
            continue
        per_scenario[(scenario, category)] += 1
        for name, hit in (('primary', primary), ('challenger', challenger)):
            record = scored[(name, category)]
            record['count'] += 1
            record['hits'] += hit
            record['correct'] += (hit == (category == 1))
    hit = scored[('primary', 1)]
    miss = scored[('primary', 2)]
    hit_rate = hit['correct']/hit['count'] if hit['count'] else None
    false_hit_rate = miss['hits']/miss['count'] if miss['count'] else None
    rejection_rate = rejection['unavailable']/rejection['eligible_nonstationary'] if rejection['eligible_nonstationary'] else None
    gates = {
        'primary_clear_hit_accuracy_99pct': hit_rate is not None and hit_rate >= .99,
        'primary_clear_miss_false_hit_max_1pct': false_hit_rate is not None and false_hit_rate <= .01,
        'overall_samples_100_per_clear_category': hit['count'] >= 100 and miss['count'] >= 100,
        'per_scenario_25_per_clear_category': all(per_scenario[(s, c)] >= 25 for s in SCENARIOS for c in (1, 2)),
        'nonstationary_unavailable_max_5pct': rejection_rate is not None and rejection_rate <= .05,
        'cover_10_occluded_zero_incorrect': cover_count >= 10 and cover_bad == 0,
        'reverse_timing_all_verified': reverse_total >= 50 and reverse_near == reverse_total,
        'no_invalid_or_unresolved_records': not errors,
    }
    return {
        'target_rtt_ms': band, 'overall': 'PASS' if all(gates.values()) else 'FAIL_OR_INCOMPLETE',
        'gates': gates, 'rates': {'hit_accuracy': hit_rate, 'miss_false_hit': false_hit_rate, 'unavailable_nonstationary': rejection_rate},
        'samples': {'server_records': len(shots), 'far_miss_excluded_client': excluded, 'statuses': dict(classifications),
                    'by_scenario_and_category': {f'{s}_{c}': per_scenario[(s, c)] for s in SCENARIOS for c in (1, 2)},
                    'occluded_cover_clear_hits': cover_count, 'incorrect_cover_outcomes': cover_bad,
                    'reverse_near_turn': reverse_near, 'reverse_total': reverse_total,
                    'primary': {str(c): scored[('primary', c)] for c in (1, 2)},
                    'challenger': {str(c): scored[('challenger', c)] for c in (1, 2)}},
        'errors': errors,
        'limitations': ['Client and server lines must be in a shared time-aligned log; separate PIE logs need explicit clock alignment.',
                        'Key collisions across shooters/sessions cause conservative reversal failure.',
                        'Only labeled strafe/reverse shots enter the nonstationary rejection denominator; independent proof of motion is required. Crouch is omitted because the label does not establish motion.',
                        'Test setup and true configured RTT require external verification.']
    }

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--target-rtt', type=int, choices=(50, 100, 150), required=True)
    p.add_argument('--json', action='store_true')
    p.add_argument('log')
    args = p.parse_args()
    result = analyze(args.log, args.target_rtt)
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"D1.1C band {args.target_rtt} ms: {result['overall']}")
        for name, valid in result['gates'].items():
            print(f"  {'PASS' if valid else 'FAIL'} {name}")
        print('Rates:', result['rates'])
        print('Samples:', result['samples'])
        for e in result['errors'][:40]:
            print('ERROR:', e)
    return 0 if result['overall'] == 'PASS' else 1

if __name__ == '__main__':
    sys.exit(main())
