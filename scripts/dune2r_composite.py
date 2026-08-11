"""Small, dependency-light compositor shared by the Dune2R mount packager."""

from __future__ import annotations

from typing import Any

from PIL import Image


def _alpha_union_bbox(frames: list[Image.Image]) -> tuple[int, int, int, int] | None:
    union = None
    for frame in frames:
        bbox = frame.getchannel("A").getbbox()
        if bbox is None:
            continue
        if union is None:
            union = bbox
        else:
            union = (
                min(union[0], bbox[0]),
                min(union[1], bbox[1]),
                max(union[2], bbox[2]),
                max(union[3], bbox[3]),
            )
    return union


def _prepare_layer(layer: dict[str, Any]) -> dict[str, Any]:
    frames = [frame.convert("RGBA") for frame in layer["frames"]]
    first_bbox = frames[0].getchannel("A").getbbox()
    union_bbox = _alpha_union_bbox(frames)
    if union_bbox is not None:
        frames = [frame.crop(union_bbox) for frame in frames]
    if first_bbox is not None and union_bbox is not None:
        pivot_x = (first_bbox[0] + first_bbox[2]) / 2.0 - union_bbox[0]
        pivot_y = (first_bbox[1] + first_bbox[3]) / 2.0 - union_bbox[1]
    else:
        pivot_x = frames[0].width / 2.0
        pivot_y = frames[0].height / 2.0

    normalization = 1.0
    reference = layer.get("reference_bbox_size")
    if first_bbox is not None and isinstance(reference, (list, tuple)) and len(reference) >= 2:
        width = max(1, first_bbox[2] - first_bbox[0])
        height = max(1, first_bbox[3] - first_bbox[1])
        normalization = ((max(1, int(reference[0])) / width)
                         * (max(1, int(reference[1])) / height)) ** 0.5
    render_scale = max(0.1, min(8.0, normalization * float(layer.get("scale") or 1.0)))
    if abs(render_scale - 1.0) >= 0.001:
        frames = [frame.resize(
            (max(1, round(frame.width * render_scale)),
             max(1, round(frame.height * render_scale))),
            Image.Resampling.NEAREST,
        ) for frame in frames]
        pivot_x *= render_scale
        pivot_y *= render_scale
    return {
        **layer,
        "frames": frames,
        "pivot_x": pivot_x,
        "pivot_y": pivot_y,
        "offset_x": int(layer.get("offset_x") or 0),
        "offset_y": int(layer.get("offset_y") or 0),
    }


def compose_layers(layers: list[dict[str, Any]], max_frames: int = 240) -> list[Image.Image]:
    """Compose aligned authoring layers without flattening their source assets."""
    prepared = [_prepare_layer(layer) for layer in layers]
    if not prepared:
        return []

    # Authoring frames are commonly 1000px or larger. Runtime atlases are
    # much smaller, so reduce every layer by one shared factor before doing
    # frame-by-frame composition. A shared factor preserves relative scale.
    longest = max(max(frame.width, frame.height)
                  for layer in prepared for frame in layer["frames"])
    if longest > 512:
        reduction = 512.0 / longest
        for layer in prepared:
            layer["frames"] = [frame.resize(
                (max(1, round(frame.width * reduction)),
                 max(1, round(frame.height * reduction))),
                Image.Resampling.LANCZOS,
            ) for frame in layer["frames"]]
            layer["pivot_x"] *= reduction
            layer["pivot_y"] *= reduction

    anchor_width = max(1, prepared[0]["frames"][0].width)
    anchor_height = max(1, prepared[0]["frames"][0].height)
    for layer in prepared:
        reference = layer.get("offset_reference_size")
        if isinstance(reference, (list, tuple)) and len(reference) >= 2:
            layer["offset_x"] = round(layer["offset_x"] * anchor_width / max(1, int(reference[0])))
            layer["offset_y"] = round(layer["offset_y"] * anchor_height / max(1, int(reference[1])))

    left = min(round(-layer["pivot_x"] + layer["offset_x"]) for layer in prepared)
    top = min(round(-layer["pivot_y"] + layer["offset_y"]) for layer in prepared)
    right = max(round(layer["frames"][0].width - layer["pivot_x"] + layer["offset_x"])
                for layer in prepared)
    bottom = max(round(layer["frames"][0].height - layer["pivot_y"] + layer["offset_y"])
                 for layer in prepared)
    padding = 12
    width = max(1, right - left + padding * 2)
    height = max(1, bottom - top + padding * 2)
    count = max(1, min(max_frames, max(len(layer["frames"]) for layer in prepared)))

    output = []
    for index in range(count):
        canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        for layer in prepared:
            frames = layer["frames"]
            frame = frames[min(len(frames) - 1, index * len(frames) // count)]
            x = round(padding - left - layer["pivot_x"] + layer["offset_x"])
            y = round(padding - top - layer["pivot_y"] + layer["offset_y"])
            canvas.alpha_composite(frame, (x, y))
        output.append(canvas)
    return output
