"""Pack a Higgsfield-rendered 6.32 second film into native WIC JPEG frames.

Usage: python tools/pack_boot_film.py build/higgsfield/boot_intro.mp4
Only the generated ARVF is shipped; the editable scene remains in Higgsfield.
"""
import argparse
from contextlib import contextmanager
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess
import uuid


@contextmanager
def workspace_frames():
    # Inherit workspace ACLs. Windows Python's private temporary-directory ACL
    # can prevent a sandboxed FFmpeg child from writing to its parent's folder.
    root = (Path(__file__).resolve().parent.parent / "build" / "higgsfield").resolve()
    root.mkdir(parents=True, exist_ok=True)
    folder = root / ("pack-" + uuid.uuid4().hex)
    folder.mkdir()
    try:
        yield folder
    finally:
        if folder.resolve().parent != root:
            raise RuntimeError("Refusing cleanup outside the generated-frames directory")
        shutil.rmtree(folder)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--output", type=Path, default=Path("assets/boot_intro.arvf"))
    args = parser.parse_args()
    if not args.source.is_file():
        parser.error("source video does not exist")
    with workspace_frames() as folder:
        frames = Path(folder)
        subprocess.run([
            "ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(args.source),
            "-an", "-vf", "fps=30,scale=960:540:flags=lanczos", "-frames:v", "190",
            "-q:v", "3", "-start_number", "0", str(frames / "%04d.jpg")
        ], check=True)
        paths = sorted(frames.glob("*.jpg"))
        if len(paths) != 190:
            raise SystemExit(f"Expected 190 frames, got {len(paths)}; refusing incomplete film")
        payloads = [p.read_bytes() for p in paths]
        if any(not b.startswith(b"\xff\xd8") or not b.endswith(b"\xff\xd9") for b in payloads):
            raise SystemExit("Invalid JPEG payload")
        header = struct.pack("<6I", 0x46565241, 1, 960, 540, 30, len(payloads))
        offset = len(header) + 8 * len(payloads)
        index = []
        for payload in payloads:
            index.append(struct.pack("<2I", offset, len(payload)))
            offset += len(payload)
        packed = header + b"".join(index) + b"".join(payloads)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(packed)
        print(f"ARVF: 190 frames, 960x540 @ 30 fps, {len(packed):,} bytes")
        print(f"SHA256: {hashlib.sha256(packed).hexdigest()}")


if __name__ == "__main__":
    main()
