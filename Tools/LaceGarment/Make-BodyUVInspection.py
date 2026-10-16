"""Generate diagnostic textures from the exported UE UV0, never from source FBX UVs.

These are Saved/ verification inputs, not product assets. Run the body commandlet
with -InspectionTexture for a transient, read-only UE render of each resolution.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("body_json", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = json.loads(args.body_json.read_text(encoding="utf-8-sig"))
    positions = np.asarray(data["positionsCm"])
    uv = np.asarray(data["uv0"])
    triangles = np.asarray(data["triangles"], dtype=int)
    args.output.mkdir(parents=True, exist_ok=False)
    y, x = np.indices((2048, 2048))
    cells = ((x // 32 + y // 32) % 2) * 190 + 45
    checker = Image.fromarray(np.repeat(cells[:, :, None], 3, axis=2).astype(np.uint8))
    paint = Image.new("RGB", (2048, 2048))
    mask = Image.new("L", (2048, 2048))
    draw, mask_draw = ImageDraw.Draw(paint), ImageDraw.Draw(mask)
    for face in triangles:
        # UE texture V is measured downward from the image's top edge.
        polygon = [tuple(p) for p in uv[face] * 2048]
        color = (230, 50, 40) if positions[face, 1].mean() < 0 else (35, 100, 240)
        draw.polygon(polygon, fill=color)
        mask_draw.polygon(polygon, fill=255)
    expanded = paint.filter(ImageFilter.MaxFilter(33))
    padding = mask.filter(ImageFilter.MaxFilter(33))
    background = Image.new("RGB", (2048, 2048), (255, 0, 255))
    background.paste(expanded, mask=padding)
    background.paste(paint, mask=mask)
    files = []
    for size in (2048, 1024, 512):
        for name, image in (("checker", checker), ("sides", background)):
            filename = f"{name}-{size}.png"
            image.resize((size, size), Image.Resampling.LANCZOS).save(args.output / filename)
            files.append(filename)
    report = {
        "sourceBodyJson": str(args.body_json.resolve()),
        "sourceSha256": hashlib.sha256(args.body_json.read_bytes()).hexdigest(),
        "checkerCellPixelsAt2048": 32,
        "sideColors": "negative mesh Y red; positive mesh Y blue",
        "paddingPixelsAt2048": 16,
        "background": "magenta reveals sampling outside the painted and padded UV regions",
        "resampling": "explicit Lanczos resize; UE inspection uses bilinear sampling, not automatic mip generation",
        "files": files,
    }
    (args.output / "inputs.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(args.output.resolve())


if __name__ == "__main__":
    main()
