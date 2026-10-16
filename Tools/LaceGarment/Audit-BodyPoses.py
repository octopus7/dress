"""Audit UE read-only pose captures. Numerical checks do not replace visual QA."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def direction(bones, start, end):
    vector = np.array(bones[end]["posedOriginCm"]) - bones[start]["posedOriginCm"]
    length = np.linalg.norm(vector)
    if length < 1e-8:
        raise ValueError(f"Collapsed bone segment: {start} -> {end}")
    return vector / length


def angle(a, b):
    return float(np.degrees(np.arccos(np.clip(np.dot(a, b), -1, 1))))


def audit(reference, inspection):
    rest = np.asarray(reference["positionsCm"], dtype=float)
    points = np.asarray(inspection["skinnedPositionsCm"], dtype=float)
    faces = np.asarray(reference["triangles"], dtype=int)
    geometry_bytes = np.column_stack([rest, reference["uv0"]]).astype("<f4").tobytes() + faces.astype("<u4").tobytes()
    geometry_hash = hashlib.sha1(geometry_bytes).hexdigest()
    geometry_matches = inspection.get("referenceGeometrySha1") == geometry_hash if "referenceGeometrySha1" in inspection else None
    if geometry_matches is False:
        raise ValueError("Reference vertex ordering, positions, UV0 or topology changed; use a matching reference report")
    if points.shape != rest.shape or not np.isfinite(points).all():
        raise ValueError("Pose vertices do not match the finite reference geometry")
    if inspection["assetsSaved"] or not inspection["poseDataValid"] or not inspection["rendered"]:
        raise ValueError("Missing read-only render or invalid pose data")
    bones = {b["name"]: b for b in inspection["bones"]}
    pose = inspection["inspectionPose"]
    measurements = {}
    checks = []
    for side in ("l", "r"):
        upper = direction(bones, f"upperarm_{side}", f"forearm_{side}")
        lower = direction(bones, f"forearm_{side}", f"hand_{side}")
        thigh = direction(bones, f"thigh_{side}", f"calf_{side}")
        calf = direction(bones, f"calf_{side}", f"foot_{side}")
        if pose in ("ArmsRaised", "ArmsForward"):
            target = [0, -1 if side == "l" else 1, 0] if pose == "ArmsRaised" else [1, 0, 0]
            error = angle(upper, target)
            measurements[f"upperarm_{side}_directionErrorDegrees"] = error
            checks.append(error < .01)
        if pose == "Elbows90":
            degrees = angle(upper, lower)
            measurements[f"elbow_{side}_degrees"] = degrees
            checks.append(abs(degrees - 90) < .01)
        if pose in ("Knees90", "Sitting"):
            degrees = angle(thigh, calf)
            measurements[f"knee_{side}_degrees"] = degrees
            checks.append(abs(degrees - 90) < .01)
        if pose == "Sitting":
            error = max(angle(thigh, [1, 0, 0]), angle(calf, [0, 0, -1]))
            measurements[f"sitting_{side}_directionErrorDegrees"] = error
            checks.append(error < .01)
    motion = float(np.linalg.norm(points - rest, axis=1).max())
    if pose == "Reference":
        checks.append(motion < 1e-4)
    else:
        checks.append(motion > .01)
    if pose == "HeadTurn":
        q = np.asarray(bones["head"]["posedRotation"])
        r = np.asarray(bones["head"]["referenceRotation"])
        degrees = float(np.degrees(2 * np.arccos(np.clip(abs(np.dot(q, r)), 0, 1))))
        measurements["head_rotationDegrees"] = degrees
        checks.append(abs(degrees - 45) < .01)
    def areas(vertices):
        triangles = vertices[faces]
        return np.linalg.norm(np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]), axis=1) / 2
    reference_area, posed_area = areas(rest), areas(points)
    if np.any(reference_area <= 1e-12):
        raise ValueError("Degenerate reference triangle")
    ratios = posed_area / reference_area
    collapsed = int(np.count_nonzero(posed_area <= 1e-12))
    return {
        "pose": pose,
        "referenceGeometryMatches": geometry_matches,
        "poseReached": bool(checks and all(checks)),
        "measurements": measurements,
        "vertices": len(points),
        "triangles": len(faces),
        "maxVertexMotionCm": motion,
        "collapsedTriangles": collapsed,
        "areaRatioMinP01P50P99Max": np.quantile(ratios, [0, .01, .5, .99, 1]).tolist(),
        "poseAndGeometryChecksPassed": bool(checks and all(checks) and collapsed == 0),
        "numericalChecksPassed": bool(checks and all(checks) and collapsed == 0 and inspection.get("posedCollisionRead") and inspection.get("uncoveredBodySamples", -1) == 0),
        "posedCollisionSamplesCovered": bool(inspection.get("posedCollisionRead") and inspection.get("uncoveredBodySamples", -1) == 0),
        "uncoveredBodySamples": inspection.get("uncoveredBodySamples"),
        "maxUncoveredBodyGapCm": inspection.get("maxUncoveredBodyGapCm"),
        "worstUncoveredBodyPositionCm": inspection.get("worstUncoveredBodyPositionCm"),
        "visualJointAndCollisionAcceptance": "not asserted; inspect rendered joints and posed body collision separately",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference_body_json", type=Path)
    parser.add_argument("inspection_json", type=Path, nargs="+")
    args = parser.parse_args()
    reference = read(args.reference_body_json)
    passed = True
    for path in args.inspection_json:
        result = audit(reference, read(path))
        output = path.with_name("pose-audit.json")
        output.write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps(result))
        passed &= result["numericalChecksPassed"]
    raise SystemExit(0 if passed else 1)
