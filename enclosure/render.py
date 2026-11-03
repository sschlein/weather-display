"""Renders the case's STLs into enclosure/case.png: ray-traced orthographic
views of the front, the front at an angle, the case open, and the lid from
the wall's side. No OpenGL needed.

    uv run --no-project --python 3.12 --with trimesh --with rtree --with pillow \\
        --with numpy --with scipy python enclosure/render.py
"""

from pathlib import Path

import numpy as np
import trimesh
from PIL import Image

HERE = Path(__file__).resolve().parent
COLORS = {"front": (58, 58, 62), "sun": (245, 197, 66), "cloud": (240, 240, 240), "back": (110, 110, 118)}


def load(names, shift=None):
    """The parts as they hang on the wall. The front and its inlays come
    lying face down, as they print: flip them back."""
    front = trimesh.load(HERE / "front.stl")
    (_, y0, _), (_, y1, z1) = front.bounds
    meshes = []
    for name in names:
        mesh = trimesh.load(HERE / f"{name}.stl")
        if name != "back":
            flip = np.eye(4)
            flip[1, 1], flip[2, 2] = -1, -1
            flip[1, 3], flip[2, 3] = y0 + y1, z1
            mesh.apply_transform(flip)
        if shift and name in shift:
            mesh.apply_translation(shift[name])
        meshes.append((name, mesh))
    return meshes


def render(meshes, direction, up, res=0.25, pad=6):
    direction = np.array(direction, float)
    direction /= np.linalg.norm(direction)
    right = np.cross(direction, np.array(up, float))
    right /= np.linalg.norm(right)
    up = np.cross(right, direction)
    points = np.vstack([m.vertices for _, m in meshes])
    center = points.mean(axis=0)
    pr, pu = (points - center) @ right, (points - center) @ up
    w, h = int((np.ptp(pr) + 2 * pad) / res), int((np.ptp(pu) + 2 * pad) / res)
    gx, gy = np.meshgrid(pr.min() - pad + (np.arange(w) + 0.5) * res, pu.max() + pad - (np.arange(h) + 0.5) * res)
    origins = center + gx.reshape(-1, 1) * right + gy.reshape(-1, 1) * up - direction * 500
    rays = np.tile(direction, (len(origins), 1))
    depth = np.full(len(origins), np.inf)
    image = np.full((len(origins), 3), 255.0)
    light = -direction + 0.5 * up - 0.3 * right
    light /= np.linalg.norm(light)
    for name, mesh in meshes:
        hits, ray, tri = mesh.ray.intersects_location(origins, rays, multiple_hits=False)
        dist = np.linalg.norm(hits - origins[ray], axis=1)
        closer = dist < depth[ray]
        ray, tri, dist = ray[closer], tri[closer], dist[closer]
        depth[ray] = dist
        shade = 0.35 + 0.65 * np.abs(mesh.face_normals[tri] @ light)
        image[ray] = np.array(COLORS[name])[None, :] * shade[:, None]
    return Image.fromarray(np.clip(image, 0, 255).astype(np.uint8).reshape(h, w, 3))


def main():
    front = ["front", "sun", "cloud"]
    views = [
        render(load(front), (0, 0, -1), (0, 1, 0)),
        render(load(front), (-0.45, -0.5, -1), (0, 1, 0)),
        render(load(front + ["back"], {"back": (0, 0, -30)}), (0.5, 0.35, 1), (0, 1, 0)),
        render(load(["back"]), (0, 0, 1), (0, 1, 0)),
    ]
    cell_w = max(v.width for v in views)
    top = max(views[0].height, views[1].height)
    canvas = Image.new("RGB", (cell_w * 2, top + max(views[2].height, views[3].height)), "white")
    for i, view in enumerate(views):
        canvas.paste(view, ((i % 2) * cell_w, (i // 2) * top))
    canvas.save(HERE / "case.png")
    print(HERE / "case.png")


if __name__ == "__main__":
    main()
