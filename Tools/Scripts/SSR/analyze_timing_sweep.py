#!/usr/bin/env python3
"""Score server-anchor timing candidates (t_r - RTT - d) against labeled shots.

Joins each `SSRGameplay SERVER` record (shooter-side intent label: category
1=clear_hit, 2=clear_miss, 3=near_miss) with the `SSRTimingSweep SERVER`
record for the same shot sequence, then reports per-d accuracy. The
echoed-stamp measurement (`stampHit`) from the same record is scored too, as
a reference for what per-target presentation provenance achieves.

When the server ran with SSR active (`Enclave.Debug.SSRSelection 1`), the
authoritative `SSRSelection SERVER` records are scored too. Present-time
fallbacks are reported separately (their outcome is not in the record).

Unavailable sweep samples are counted as errors, never dropped.

Usage: python analyze_timing_sweep.py server.log [more.log ...] [--json]
"""
import argparse
import collections
import json
import re
import sys

FIELD = re.compile(r'([A-Za-z][A-Za-z0-9]*)=([^\s()]+)')
HIT, MISS = 1, 2  # EEnclaveSSRQueryOutcome::HistoricalHit / HistoricalMiss


def fields(line):
    return dict(FIELD.findall(line))


def load(paths):
    labels, sweeps, selections, rtts = {}, {}, {}, []
    for path in paths:
        with open(path, encoding='utf-8-sig', errors='replace') as file:
            for line in file:
                if 'SSRGameplay SERVER ' in line:
                    f = fields(line)
                    if 'key' in f and 'category' in f:
                        labels[(path, f['key'])] = f
                        if f.get('rttMs'):
                            rtts.append(float(f['rttMs']))
                elif 'SSRSelection SERVER ' in line:
                    f = fields(line)
                    if 'seq' in f:
                        selections[(path, f['seq'])] = f
                elif 'SSRTimingSweep SERVER ' in line:
                    f = fields(line)
                    if 'seq' in f and 'sampleOutcomes' in f:
                        samples = {}
                        for pair in f['sampleOutcomes'].split(','):
                            delay, outcome = pair.split(':')
                            samples[int(float(delay))] = int(outcome)
                        sweeps[(path, f['seq'])] = samples
    return labels, sweeps, selections, rtts


def score(labels, sweeps):
    per_delay = collections.defaultdict(lambda: collections.Counter())
    stamp = collections.Counter()
    unmatched = 0
    for key, label in labels.items():
        category = int(label['category'])
        if category not in (1, 2):
            continue
        want_hit = category == 1
        name = 'clear_hit' if want_hit else 'clear_miss'
        if label.get('status') == 'measured':
            stamp[name + '_n'] += 1
            stamp[name + '_ok'] += (label.get('stampHit') == '1') == want_hit
        sweep = sweeps.get(key)
        if sweep is None:
            unmatched += 1
            continue
        for delay, outcome in sweep.items():
            c = per_delay[delay]
            c[name + '_n'] += 1
            if outcome not in (HIT, MISS):
                c[name + '_unavailable'] += 1
            elif (outcome == HIT) == want_hit:
                c[name + '_ok'] += 1
    return per_delay, stamp, unmatched


HISTORICAL, FALLBACK = '3', '4'  # EEnclaveSSRSelection::Historical / PresentTimeUnavailableFallback


def score_selection(labels, selections):
    c = collections.Counter()
    for key, label in labels.items():
        category = int(label['category'])
        if category not in (1, 2):
            continue
        name = 'clear_hit' if category == 1 else 'clear_miss'
        sel = selections.get(key)
        if sel is None:
            c['unmatched'] += 1
            continue
        c[name + '_n'] += 1
        c['stamp_used'] += sel.get('stampUsed') == '1'
        c['stamp_rejected'] += sel.get('stampRejected') == '1'
        if sel.get('selection') != HISTORICAL:
            c[name + '_fallback'] += 1
        elif (sel.get('hit') == '1') == (category == 1):
            c[name + '_ok'] += 1
    return c


def pct(ok, n):
    return round(100.0 * ok / n, 1) if n else None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('logs', nargs='+')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    labels, sweeps, selections, rtts = load(args.logs)
    per_delay, stamp, unmatched = score(labels, sweeps)
    sel = score_selection(labels, selections) if selections else None
    rows = []
    for delay in sorted(per_delay):
        c = per_delay[delay]
        rows.append({
            'delayMs': delay,
            'clearHitN': c['clear_hit_n'], 'clearHitPct': pct(c['clear_hit_ok'], c['clear_hit_n']),
            'clearMissN': c['clear_miss_n'], 'clearMissPct': pct(c['clear_miss_ok'], c['clear_miss_n']),
            'unavailable': c['clear_hit_unavailable'] + c['clear_miss_unavailable'],
            'combinedPct': pct(c['clear_hit_ok'] + c['clear_miss_ok'], c['clear_hit_n'] + c['clear_miss_n']),
        })
    rtts.sort()
    summary = {
        'labeledShots': len(labels), 'sweepRecords': len(sweeps), 'unmatchedLabels': unmatched,
        'rttMs': {'min': rtts[0], 'median': rtts[len(rtts) // 2], 'max': rtts[-1]} if rtts else None,
        'echoedStamp': {
            'clearHitN': stamp['clear_hit_n'], 'clearHitPct': pct(stamp['clear_hit_ok'], stamp['clear_hit_n']),
            'clearMissN': stamp['clear_miss_n'], 'clearMissPct': pct(stamp['clear_miss_ok'], stamp['clear_miss_n']),
        },
        'serverAnchor': rows,
        'authoritative': None if sel is None else {
            'source': next(iter(selections.values())).get('source'),
            'clearHitN': sel['clear_hit_n'], 'clearHitPct': pct(sel['clear_hit_ok'], sel['clear_hit_n']),
            'clearMissN': sel['clear_miss_n'], 'clearMissPct': pct(sel['clear_miss_ok'], sel['clear_miss_n']),
            'presentTimeFallbacks': sel['clear_hit_fallback'] + sel['clear_miss_fallback'],
            'stampUsed': sel['stamp_used'], 'stampRejected': sel['stamp_rejected'], 'unmatched': sel['unmatched'],
        },
    }
    if args.json:
        json.dump(summary, sys.stdout, indent=2)
        print()
        return
    print(f"labeled={summary['labeledShots']} sweeps={summary['sweepRecords']} unmatched={unmatched} rttMs={summary['rttMs']}")
    e = summary['echoedStamp']
    print(f"echoed stamp:  clear_hit {e['clearHitPct']}% (n={e['clearHitN']})  clear_miss {e['clearMissPct']}% (n={e['clearMissN']})")
    a = summary['authoritative']
    if a:
        print(f"AUTHORITATIVE {a['source']}:  clear_hit {a['clearHitPct']}% (n={a['clearHitN']})  "
              f"clear_miss {a['clearMissPct']}% (n={a['clearMissN']})  fallbacks={a['presentTimeFallbacks']}  "
              f"stampUsed={a['stampUsed']} stampRejected={a['stampRejected']} unmatched={a['unmatched']}")
    print('server anchor t_r - RTT - d:')
    for r in rows:
        print(f"  d={r['delayMs']:>3}ms  clear_hit {r['clearHitPct']}% (n={r['clearHitN']})  "
              f"clear_miss {r['clearMissPct']}% (n={r['clearMissN']})  unavailable={r['unavailable']}  combined {r['combinedPct']}%")


if __name__ == '__main__':
    main()
