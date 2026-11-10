#!/usr/bin/env python3
"""Prepare an upstream merge locally; publishing happens only after validation."""

import argparse
import json
import os
import subprocess
from pathlib import Path


def git(*args, check=True):
    return subprocess.run(
        ["git", *args], check=check, text=True, stdout=subprocess.PIPE
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--event", choices=["push", "schedule", "workflow_dispatch"], required=True)
    parser.add_argument("--upstream", default="https://github.com/asmvik/yabai.git")
    parser.add_argument("--output", default=os.environ.get("GITHUB_OUTPUT"))
    args = parser.parse_args()

    git("diff", "--quiet", "HEAD", "--")
    base = git("rev-parse", "HEAD").stdout.strip()
    git("fetch", "--no-tags", args.upstream, "master:refs/remotes/upstream/master")
    upstream = git("rev-parse", "refs/remotes/upstream/master").stdout.strip()
    current = git("merge-base", "--is-ancestor", upstream, "HEAD", check=False).returncode == 0

    if not current:
        merge = git("merge", "--no-edit", upstream, check=False)
        print(merge.stdout, end="")
        if merge.returncode:
            if git("rev-parse", "--verify", "MERGE_HEAD", check=False).returncode == 0:
                git("merge", "--abort")
            raise SystemExit(
                "::error::Upstream merge failed. Nothing was pushed; master and the last release are unchanged."
            )

    values = {
        "base_sha": base,
        "upstream_sha": upstream,
        "source_sha": git("rev-parse", "HEAD").stdout.strip(),
        "build_required": str(args.event != "schedule" or not current).lower(),
    }
    print(json.dumps(values, indent=2))
    if args.output:
        with Path(args.output).open("a") as stream:
            for key, value in values.items():
                stream.write(f"{key}={value}\n")


if __name__ == "__main__":
    main()
