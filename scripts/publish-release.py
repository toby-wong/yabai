#!/usr/bin/env python3
"""Publish validated source and binaries, then update the prebuilt Homebrew formula."""

import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--expected-head", required=True)
    args = parser.parse_args()
    repository = os.environ["GITHUB_REPOSITORY"]
    if repository != "toby-wong/yabai":
        raise SystemExit("Publishing is restricted to toby-wong/yabai")

    info = json.loads(args.manifest.read_text())
    if not re.fullmatch(r"[0-9]+\.[0-9]+", info["build_id"]):
        raise SystemExit("Only release builds can be published")
    if run("git", "rev-parse", "HEAD") != info["source_sha"]:
        raise SystemExit("The checkout no longer matches the tested binary")
    subprocess.run(["git", "diff", "--quiet", "HEAD", "--"], check=True)
    archive = args.manifest.parent / info["archive"]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != info["sha256"]:
        raise SystemExit("Archive checksum changed after validation")

    subprocess.run(["git", "fetch", "origin", "master"], check=True)
    if run("git", "rev-parse", "FETCH_HEAD") != args.expected_head:
        raise SystemExit("master changed during the build; rerun against its latest commit")

    # A normal fast-forward push is intentional: concurrent changes are never overwritten.
    subprocess.run(["git", "push", "origin", "HEAD:refs/heads/master"], check=True)
    notes = args.manifest.parent / "release-notes.md"
    notes.write_text(
        f"Native macOS tabs share one tile when `window_native_tabs on` is enabled.\n\n"
        f"- Source: `{info['source_sha']}`\n"
        f"- Upstream ancestor: `{info['upstream_sha']}`\n"
        f"- Binary: `{info['version_output']}`\n"
        "- Universal Apple Silicon and Intel binary, built and tested in GitHub Actions.\n"
        "- Ad-hoc signed; no Developer ID signing or notarization is claimed.\n"
        "- Includes the existing macOS-version and hide-on-close fixes.\n\n"
        "See `FORK.md` for provenance, installation, updates, and rollback.\n"
    )
    subprocess.run([
        "gh", "release", "create", info["tag"],
        str(archive), str(args.manifest.parent / "SHA256SUMS"), str(args.manifest),
        "--repo", repository, "--target", info["source_sha"],
        "--title", f"yabai {info['base_version']} — Toby build {info['build_id']}",
        "--notes-file", str(notes), "--draft",
    ], check=True)
    subprocess.run([
        "gh", "release", "edit", info["tag"], "--repo", repository, "--draft=false", "--latest"
    ], check=True)

    url = f"https://github.com/{repository}/releases/download/{info['tag']}/{info['archive']}"
    formula = f'''class Yabai < Formula
  desc "Tiling window manager with native macOS tab support"
  homepage "https://github.com/{repository}"
  url "{url}"
  version "{info['package_version']}"
  sha256 "{info['sha256']}"
  license "MIT"

  depends_on macos: :big_sur

  def install
    bin.install "bin/yabai"
    man1.install "doc/yabai.1"
    (pkgshare/"examples").install "examples/yabairc", "examples/skhdrc"
    pkgshare.install "build-info.json", "FORK.md"
  end

  def caveats
    <<~EOS
      This formula installs the prebuilt Apple Silicon/Intel release.
      Enable native tabs in your yabairc:
        yabai -m config window_native_tabs on

      Start at login: yabai --start-service
      After upgrading: yabai --restart-service
      Grant Accessibility access to yabai if macOS requests it.
    EOS
  end

  test do
    assert_match "{info['version_output']}", shell_output("#{{bin}}/yabai --version")
  end
end
'''
    path = Path("Formula/yabai.rb")
    path.parent.mkdir(exist_ok=True)
    path.write_text(formula)
    subprocess.run(["ruby", "-c", str(path)], check=True)
    subprocess.run(["git", "add", str(path)], check=True)
    subprocess.run([
        "git", "commit", "-m", f"Update prebuilt Homebrew package to {info['package_version']}"
    ], check=True)
    subprocess.run(["git", "push", "origin", "HEAD:refs/heads/master"], check=True)
    release_url = f"https://github.com/{repository}/releases/tag/{info['tag']}"
    print(release_url)
    if summary := os.environ.get("GITHUB_STEP_SUMMARY"):
        with Path(summary).open("a") as stream:
            stream.write(f"Published [{info['version_output']}]({release_url}) and updated `Formula/yabai.rb`.\n")


if __name__ == "__main__":
    main()
