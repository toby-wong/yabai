#!/usr/bin/env python3
"""Build and package a universal binary without installing or starting yabai."""

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import tarfile
import tempfile
from datetime import datetime, timezone
from pathlib import Path


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-id", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"(?:ci\.)?[0-9]+\.[0-9]+", args.build_id):
        parser.error("build-id must be RUN.ATTEMPT or ci.RUN.ATTEMPT")

    subprocess.run(["git", "diff", "--quiet", "HEAD", "--"], check=True)
    source_sha = run("git", "rev-parse", "HEAD")
    source = Path("src/yabai.c").read_text()
    base = ".".join(re.search(rf"^#define {part}\s+(\d+)$", source, re.M)[1]
                    for part in ("MAJOR", "MINOR", "PATCH"))
    suffix = f"-toby.{args.build_id}+{source_sha[:8]}"
    version_output = f"yabai-v{base}{suffix}"
    upstream = subprocess.run(
        ["git", "merge-base", "HEAD", "refs/remotes/upstream/master"],
        text=True, capture_output=True
    )

    # Upstream's install target compiles into bin/; it does not install on the host.
    subprocess.run(
        ["make", "-j1", "install", f'CLI_FLAGS=-DYABAI_BUILD_SUFFIX=\\"{suffix}\\"'],
        check=True,
    )
    subprocess.run(
        ["codesign", "--force", "--sign", "-", "--identifier", "com.asmvik.yabai", "bin/yabai"],
        check=True,
    )
    subprocess.run(["codesign", "--verify", "--strict", "bin/yabai"], check=True)
    architectures = run("lipo", "-archs", "bin/yabai").split()
    if set(architectures) != {"arm64", "x86_64"}:
        raise SystemExit(f"Unexpected architectures: {architectures}")
    if run("./bin/yabai", "--version") != version_output:
        raise SystemExit("Built binary does not report the expected fork version")

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    archive_name = f"yabai-v{base}-toby.{args.build_id}.tar.gz"
    info = {
        "base_version": base,
        "build_id": args.build_id,
        "package_version": f"{base}.{args.build_id}",
        "version_output": version_output,
        "tag": f"toby-v{base}-r{args.build_id}",
        "source_sha": source_sha,
        "upstream_sha": upstream.stdout.strip() if upstream.returncode == 0 else None,
        "built_at": datetime.now(timezone.utc).isoformat(),
        "architectures": architectures,
        "signature": "ad-hoc",
        "archive": archive_name,
    }

    with tempfile.TemporaryDirectory(prefix="yabai-package-") as directory:
        root = Path(directory) / "archive"
        (root / "bin").mkdir(parents=True)
        shutil.copy2("bin/yabai", root / "bin/yabai")
        shutil.copytree("doc", root / "doc")
        shutil.copytree("examples", root / "examples")
        for name in ("LICENSE.txt", "FORK.md"):
            shutil.copy2(name, root / name)
        (root / "build-info.json").write_text(json.dumps(info, indent=2) + "\n")
        with tarfile.open(output / archive_name, "w:gz") as archive:
            archive.add(root, arcname="archive")

    info["sha256"] = hashlib.sha256((output / archive_name).read_bytes()).hexdigest()
    (output / "SHA256SUMS").write_text(f'{info["sha256"]}  {archive_name}\n')
    (output / "release.json").write_text(json.dumps(info, indent=2) + "\n")
    print(json.dumps(info, indent=2))


if __name__ == "__main__":
    main()
