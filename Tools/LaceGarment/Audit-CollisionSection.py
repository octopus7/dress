"""Development QA: compare exported UE mesh and collision sections, without DCCs."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def audit(body_path, fit_path):
    body = json.loads(body_path.read_text(encoding="utf-8-sig"))
    fit = json.loads(fit_path.read_text(encoding="utf-8-sig"))
    vertices = np.asarray(body["positionsCm"], dtype=float)
    triangles = vertices[np.asarray(body["triangles"], dtype=int)]
    origin = triangles[:, 0][:, [0, 2]]
    edge_a = triangles[:, 1][:, [0, 2]] - origin
    edge_b = triangles[:, 2][:, [0, 2]] - origin
    determinant = edge_a[:, 0] * edge_b[:, 1] - edge_a[:, 1] * edge_b[:, 0]
    valid = np.abs(determinant) > 1e-10
    sections = []
    for height in (6.6, 7.0, 7.4, 7.8, 8.0, 8.2, 8.4, 8.6, 9.0, 9.5):
        delta = np.array([0.0, height]) - origin
        a = np.zeros(len(triangles))
        b = np.zeros(len(triangles))
        a[valid] = (delta[valid, 0] * edge_b[valid, 1] - delta[valid, 1] * edge_b[valid, 0]) / determinant[valid]
        b[valid] = (edge_a[valid, 0] * delta[valid, 1] - edge_a[valid, 1] * delta[valid, 0]) / determinant[valid]
        hits = valid & (a >= -1e-8) & (b >= -1e-8) & (a + b <= 1 + 1e-8)
        y = triangles[:, 0, 1] + a * (triangles[:, 1, 1] - triangles[:, 0, 1]) + b * (triangles[:, 2, 1] - triangles[:, 0, 1])
        mesh_edges = np.unique(np.round(y[hits & (y >= 0)], 6)).tolist()
        points = np.column_stack((np.zeros(5001), np.linspace(0, 5, 5001), np.full(5001, height)))
        inside = np.zeros(5001, dtype=bool)
        for capsule in fit["capsules"]:
            start = np.asarray(capsule["aCm"])
            axis = np.asarray(capsule["bCm"]) - start
            closest = start + np.clip((points - start) @ axis / max(axis @ axis, 1e-12), 0, 1)[:, None] * axis
            inside |= np.linalg.norm(points - closest, axis=1) < capsule["radiusCm"]
        transitions = (np.flatnonzero(inside[:-1] != inside[1:]) / 1000).tolist()
        sections.append({"xCm": 0, "zCm": height, "meshPositiveYIntersectionsCm": mesh_edges,
                         "proxyInsideAtYZero": bool(inside[0]), "proxyPositiveYTransitionsCm": transitions})
    return {"scope": "Static cross-section diagnostic; not garment or pose acceptance",
            "meshMethod": "Triangle intersection with the X=0,Z=height line; parallel triangles excluded",
            "proxyMethod": "Union membership sampled every 0.001cm on positive Y, without cloth thickness",
            "bodyReportSha256": hashlib.sha256(body_path.read_bytes()).hexdigest(),
            "fitReportSha256": hashlib.sha256(fit_path.read_bytes()).hexdigest(), "sections": sections}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("body", type=Path)
    parser.add_argument("fit", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    if arguments.output.exists():
        parser.error("Refusing to overwrite an existing audit")
    report = audit(arguments.body, arguments.fit)
    arguments.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    for section in report["sections"]:
        print(section)
