"""Development QA of the geometry exported from UE, not a product dependency.

Tests positive-area UV overlaps by convex polygon clipping after a grid broad phase.
Writes measured chart areas/densities; never trusts the source FBX's sidecar report.
"""
import argparse
import json
import math
from collections import defaultdict
from pathlib import Path

import numpy as np


def cross(a, b):
    return a[0] * b[1] - a[1] * b[0]


def area(poly):
    return abs(sum(cross(poly[i], poly[(i + 1) % len(poly)]) for i in range(len(poly)))) * 0.5


def intersection_area(subject, clip):
    output = [tuple(p) for p in subject]
    if cross(clip[1] - clip[0], clip[2] - clip[0]) < 0:
        clip = clip[::-1]
    for i in range(3):
        a, b = clip[i], clip[(i + 1) % 3]
        edge = b - a
        source, output = output, []
        if not source:
            return 0.0
        prev = source[-1]
        pd = cross(edge, (prev[0] - a[0], prev[1] - a[1]))
        for point in source:
            d = cross(edge, (point[0] - a[0], point[1] - a[1]))
            if (d >= 0) != (pd >= 0):
                t = pd / (pd - d)
                output.append((prev[0] + t * (point[0] - prev[0]), prev[1] + t * (point[1] - prev[1])))
            if d >= 0:
                output.append(point)
            prev, pd = point, d
    return area(output) if len(output) >= 3 else 0.0


def boundary_distance(first, second):
    """Exact 2D segment distance, including crossing boundaries, in bounded batches."""
    best = math.inf
    def point_distance(p, a, b):
        d = b - a
        t = np.clip(np.sum((p-a)*d, axis=-1) / np.maximum(np.sum(d*d, axis=-1), 1e-30), 0, 1)
        return np.sum((p-a-t[..., None]*d)**2, axis=-1)
    for start in range(0, len(first), 128):
        a = first[start:start+128, None, 0]
        b = first[start:start+128, None, 1]
        c, d = second[None, :, 0], second[None, :, 1]
        distances = np.minimum.reduce([point_distance(a,c,d), point_distance(b,c,d), point_distance(c,a,b), point_distance(d,a,b)])
        ab, cd = b-a, d-c
        cross2 = lambda u,v: u[...,0]*v[...,1]-u[...,1]*v[...,0]
        divisor = cross2(ab,cd)
        nonparallel = np.abs(divisor)>1e-20
        safe = np.where(nonparallel,divisor,1)
        t, u = cross2(c-a,cd)/safe, cross2(c-a,ab)/safe
        if np.any(nonparallel & (t>=0) & (t<=1) & (u>=0) & (u<=1)):
            return 0.0
        best = min(best,float(distances.min()))
    return math.sqrt(best)


def audit(data):
    positions = np.asarray(data['positionsCm'], dtype=np.float64)
    uv = np.asarray(data['uv0'], dtype=np.float64)
    faces = np.asarray(data['triangles'], dtype=np.int64)
    if not np.isfinite(positions).all() or not np.isfinite(uv).all():
        raise ValueError('Non-finite imported coordinates')
    tri = uv[faces]
    xyz = positions[faces]
    du, dv = tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]
    uv_area = np.abs(du[:, 0] * dv[:, 1] - du[:, 1] * dv[:, 0]) * .5
    world_area = np.linalg.norm(np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0]), axis=1) * .5
    lo, hi = tri.min(axis=1), tri.max(axis=1)
    bins = defaultdict(list)
    size = 128
    overlaps = []
    tested = 0
    for i in range(len(faces)):
        lower = np.floor(lo[i] * size).astype(int)
        upper = np.floor(hi[i] * size).astype(int)
        candidates = set()
        for x in range(lower[0], upper[0] + 1):
            for y in range(lower[1], upper[1] + 1):
                candidates.update(bins[(x, y)])
                bins[(x, y)].append(i)
        if not candidates:
            continue
        ids = np.fromiter(candidates, dtype=np.int64)
        ids = ids[np.all(hi[ids] > lo[i] + 1e-12, axis=1) & np.all(lo[ids] < hi[i] - 1e-12, axis=1)]
        for j in ids:
            tested += 1
            overlap = intersection_area(tri[i], tri[j])
            if overlap > 1e-12:
                overlaps.append({'faces': [int(j), i], 'areaUV': overlap})

    # Chart components share a full UV edge; positions disambiguate coincident UVs.
    parent = list(range(len(faces)))
    def root(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    keys = [tuple(p) + tuple(t) for p, t in zip(positions, uv)]
    edges = {}
    edge_counts = defaultdict(int)
    edge_uv = {}
    for i, face in enumerate(faces):
        for k in range(3):
            key = tuple(sorted((keys[face[k]], keys[face[(k + 1) % 3]])))
            edge_counts[key] += 1
            edge_uv[key] = uv[[face[k],face[(k+1)%3]]]
            if key in edges:
                parent[root(i)] = root(edges[key])
            else:
                edges[key] = i
    charts = defaultdict(list)
    for i in range(len(faces)):
        charts[root(i)].append(i)
    boundaries = defaultdict(list)
    for key, count in edge_counts.items():
        if count==1:
            boundaries[root(edges[key])].append(edge_uv[key])
    boundary_arrays = [np.asarray(boundaries[chart]) for chart in charts]
    if any(len(boundary)==0 for boundary in boundary_arrays):
        raise ValueError('A UV chart has no boundary')
    minimum_gap = min((boundary_distance(a,b) for i,a in enumerate(boundary_arrays) for b in boundary_arrays[i+1:]),default=1.0)*2048
    chart_stats = []
    for ids in charts.values():
        ua, wa = float(uv_area[ids].sum()), float(world_area[ids].sum())
        chart_stats.append({'faces': len(ids), 'uvArea': ua, 'areaCm2': wa, 'pixelsPerCm': 2048 * math.sqrt(ua / wa) if wa > 0 else 0})
    density = [c['pixelsPerCm'] for c in chart_stats]
    density_ratio = max(density) / min(density) if density and min(density) > 0 else math.inf
    padding = float(min(uv.min(), 1 - uv.max()) * 2048)
    degenerate = int(np.count_nonzero((uv_area <= 1e-14) | (world_area <= 1e-14)))
    passed = not overlaps and not degenerate and padding >= 16 - 1e-4 and minimum_gap >= 32 - 1e-4 and density_ratio <= 1.02
    return {'passed': passed, 'scope': 'Imported UE LOD0 UV0; positive-area triangle clipping, chart area density, outer padding and minimum chart boundary distance. Paint seam visual QA remains separate.',
            'vertices': len(positions), 'triangles': len(faces), 'charts': len(chart_stats), 'testedPairs': tested,
            'positiveAreaOverlaps': len(overlaps), 'overlapExamples': overlaps[:20], 'degenerateTriangles': degenerate,
            'outerPaddingPixels2048': padding, 'minimumIslandGapPixels2048': minimum_gap, 'maxToMinChartDensityRatio': density_ratio, 'chartMetrics': chart_stats}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit('Refusing to overwrite an existing audit')
    result = audit(json.loads(args.input.read_text(encoding='utf-8-sig')))
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k not in ('chartMetrics', 'overlapExamples')}, ensure_ascii=False, indent=2))
    raise SystemExit(0 if result['passed'] else 1)
