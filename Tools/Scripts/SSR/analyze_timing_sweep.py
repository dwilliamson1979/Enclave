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
    labels, sweeps, selections, rtts, netstats = {}, {}, {}, [], []
    for path in paths:
        with open(path, encoding='utf-8-sig', errors='replace') as file:
            for line in file:
                if 'SSRGameplay SERVER ' in line:
                    f = fields(line)
                    if 'key' in f and 'category' in f:
                        labels[(path, f['key'])] = f
                        if f.get('rttMs'):
                            rtts.append(float(f['rttMs']))
                elif 'SSRTest NETSTATS ' in line:
                    netstats.append(fields(line))
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
    return labels, sweeps, selections, rtts, netstats


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
        # With bystanders, the nearest character on the ray may legitimately be
        # someone else; that is neither a correct nor a wrong verdict about the
        # labeled target, so it is reported separately and left out of the rates.
        claim = label.get('claimTarget')
        if claim and sel.get('hit') == '1' and sel.get('target') not in (None, claim):
            c['other_target'] += 1
            continue
        c[name + '_n'] += 1
        c['stamp_used'] += sel.get('stampUsed') == '1'
        c['stamp_rejected'] += sel.get('stampRejected') == '1'
        if sel.get('selection') != HISTORICAL:
            c[name + '_fallback'] += 1
        elif (sel.get('hit') == '1') == (category == 1):
            c[name + '_ok'] += 1
    return c


def stamp_acceptance(selections):
    """All selections (labeled or not): how echoed stamps fared and how far
    accepted ones were from the server anchor (stampAge - baseAge, ms)."""
    sent = verified = used = rejected = 0
    deltas = []
    for sel in selections.values():
        if sel.get('stampTarget', 'None') != 'None':
            sent += 1
        verified += sel.get('stampVerified') == '1'
        rejected += sel.get('stampRejected') == '1'
        if sel.get('stampUsed') == '1':
            used += 1
            try:
                deltas.append(float(sel['stampAgeMs']) - float(sel['baseAgeMs']))
            except (KeyError, ValueError):
                pass
    deltas.sort()
    return {'sent': sent, 'verified': verified, 'used': used, 'rejected': rejected,
            'usedDeltaMs': {'min': deltas[0], 'max': deltas[-1]} if deltas else None}


def pct(ok, n):
    return round(100.0 * ok / n, 1) if n else None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('logs', nargs='+')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    labels, sweeps, selections, rtts, netstats = load(args.logs)
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
            'otherTargetHits': sel['other_target'],
        },
        'stampAcceptance': stamp_acceptance(selections) if selections else None,
        'netstats': [{'playerId': n.get('playerId'), 'serverOutBytesPerSec': float(n['serverOutBytesPerSec']),
                      'serverInBytesPerSec': float(n['serverInBytesPerSec'])}
                     for n in netstats if 'serverOutBytesPerSec' in n],
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
              f"stampUsed={a['stampUsed']} stampRejected={a['stampRejected']} unmatched={a['unmatched']} "
              f"otherTargetHits={a['otherTargetHits']}")
    sa = summary['stampAcceptance']
    if sa:
        print(f"stamps: sent={sa['sent']} verified={sa['verified']} used={sa['used']} rejected={sa['rejected']} "
              f"usedDeltaMs={sa['usedDeltaMs']}")
    for n in summary['netstats']:
        print(f"netstats player {n['playerId']}: server out {n['serverOutBytesPerSec']:.0f} B/s, in {n['serverInBytesPerSec']:.0f} B/s")
    print('server anchor t_r - RTT - d:')
    for r in rows:
        print(f"  d={r['delayMs']:>3}ms  clear_hit {r['clearHitPct']}% (n={r['clearHitN']})  "
              f"clear_miss {r['clearMissPct']}% (n={r['clearMissN']})  unavailable={r['unavailable']}  combined {r['combinedPct']}%")


if __name__ == '__main__':
    main()
