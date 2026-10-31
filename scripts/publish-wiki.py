#!/usr/bin/env python3
"""Publish KinConductor Markdown documentation to the separate GitHub Wiki git repository."""
import argparse
from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
BASE = "https://github.com/RobertRupa/KinConductor"
WIKI = BASE + "/wiki"
REMOTE = BASE + ".wiki.git"
DOCS = {
 "docs/setup.md": ("Getting-Started", "Getting started"),
 "docs/profiles.md": ("Build-Profiles", "Build profiles"),
 "docs/build.md": ("Build-and-Flashing", "Build and flashing"),
 "docs/first-start.md": ("First-Start", "First start"),
 "docs/webgui.md": ("WebGUI", "WebGUI"),
 "docs/settings-reference.md": ("Settings-Reference", "Settings and parameter ranges"),
 "docs/api-reference.md": ("HTTP-API", "HTTP API"),
 "docs/hardware.md": ("Hardware", "Hardware"),
 "docs/peripherals.md": ("Peripherals", "Peripherals and automation"),
 "docs/current-measurement.md": ("Current-Measurement", "Current measurement"),
 "docs/refactoring.md": ("Architecture", "Architecture"),
 "docs/troubleshooting.md": ("Troubleshooting", "Troubleshooting"),
 "docs/releases.md": ("Releases", "Releases"),
 "docs/manufacturer-sources.md": ("Manufacturer-Resources", "Manufacturer resources"),
}
MD_LINK = re.compile(r"(!?\[[^\]\n]*\]\()([^)]+)(\))")
HEADER = "<!-- Generated from KinConductor docs by scripts/publish-wiki.py -->\n\n"

def git(*args, cwd=None, check=True, capture=False):
    return subprocess.run(["git", *args], cwd=cwd, check=check, text=True,
                          stdout=subprocess.PIPE if capture else None,
                          stderr=subprocess.PIPE if capture else None)

def convert_link(url, source, sha):
    if url.startswith(("#", "//")):
        return url
    parsed = urlsplit(url)
    if parsed.scheme or parsed.netloc or not parsed.path:
        return url
    target = (source.parent / parsed.path).resolve()
    if not target.is_relative_to(ROOT):
        return url
    relative = target.relative_to(ROOT).as_posix()
    if relative == "docs/README.md":
        out = WIKI + "/Home"
    elif relative in DOCS:
        out = WIKI + "/" + DOCS[relative][0]
    elif relative.startswith("docs/assets/"):
        out = "https://raw.githubusercontent.com/RobertRupa/KinConductor/" + sha + "/" + relative
    else:
        out = BASE + "/blob/" + sha + "/" + relative
    return out + ("#" + parsed.fragment if parsed.fragment else "")

def convert(body, source, sha):
    lines = []
    fenced = False
    for line in body.splitlines(keepends=True):
        marker = re.match(r"^\s*(?:\x60{3,}|~{3,})", line)
        if marker:
            fenced = not fenced
        elif not fenced:
            def replace(match):
                a, url, z = match.groups()
                if " " in url or url.startswith("<"):
                    return match.group(0)
                return a + convert_link(url, source, sha) + z
            line = MD_LINK.sub(replace, line)
        lines.append(line)
    return "".join(lines)

def generate():
    sha = git("rev-parse", "HEAD", cwd=ROOT, capture=True).stdout.strip()
    pages = {}
    for path, (slug, _) in DOCS.items():
        source = ROOT / path
        if not source.exists():
            raise RuntimeError("Missing documentation: " + path)
        markdown = convert(source.read_text(encoding="utf-8"), source, sha)
        pages[slug + ".md"] = (HEADER + markdown.rstrip()
          + "\n\n---\n\n[Source documentation](" + BASE + "/blob/" + sha + "/" + path + ")\n")
    sections = [
      ("Getting started", ["Getting-Started", "Build-Profiles", "Build-and-Flashing", "First-Start"]),
      ("WebGUI and configuration", ["WebGUI", "Settings-Reference", "HTTP-API", "Peripherals"]),
      ("Hardware", ["Hardware", "Current-Measurement", "Manufacturer-Resources"]),
      ("Development", ["Architecture", "Troubleshooting", "Releases"]),
    ]
    labels = {slug: label for slug, label in DOCS.values()}
    home = [HEADER, "# KinConductor Wiki\n\n",
            "KinConductor firmware for KinCony KC868-A6 ESP32-WROOM-32.\n\n",
            "Source repository: " + BASE + "\n\n",
            "**Compatibility:** other KC868-Ax profiles use A6 baseline GPIO mapping, "
            "and the coprocessor profile is not an implemented Zigbee/Thread bridge.\n\n",
            "**Safety:** confirm safe loads before flashing. Firmware and LittleFS "
            "are separate uploads; routine updates must preserve NVS.\n"]
    side = [HEADER, "# [KinConductor Wiki](" + WIKI + "/Home)\n"]
    for section, slugs in sections:
        home.append("\n## " + section + "\n\n")
        side.append("\n**" + section + "**\n\n")
        for slug in slugs:
            link = "- [" + labels[slug] + "](" + WIKI + "/" + slug + ")\n"
            home.append(link)
            side.append(link)
    pages["Home.md"] = "".join(home)
    pages["_Sidebar.md"] = "".join(side)
    return pages

def write_pages(destination, pages):
    destination.mkdir(parents=True, exist_ok=True)
    for name, content in pages.items():
        (destination / name).write_text(content, encoding="utf-8")

def publish(pages):
    with TemporaryDirectory(prefix="kinconductor-wiki-") as tmp:
        directory = Path(tmp) / "wiki"
        clone = git("clone", "--quiet", REMOTE, str(directory), capture=True, check=False)
        if clone.returncode:
            raise RuntimeError("Cannot clone Wiki. Check GitHub access or initialize "
                "the first page at " + WIKI + ". " + (clone.stderr or "").strip())
        branch = git("branch", "--show-current", cwd=directory, capture=True).stdout.strip()
        if not branch:
            branch = "master"
            git("checkout", "-B", branch, cwd=directory)
        write_pages(directory, pages)
        git("add", "--", *sorted(pages), cwd=directory)
        changed = git("diff", "--cached", "--quiet", cwd=directory, check=False)
        if changed.returncode == 0:
            print("Wiki already up to date.")
            return
        if changed.returncode != 1:
            raise RuntimeError("Could not check pending Wiki changes.")
        git("commit", "-m", "docs: synchronize KinConductor Wiki", cwd=directory)
        git("push", "origin", "HEAD:" + branch, cwd=directory)
        print("Published " + str(len(pages)) + " pages: " + WIKI)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--publish", action="store_true", help="Commit and push pages to GitHub Wiki")
    args = p.parse_args()
    try:
        pages = generate()
        if args.publish:
            print("Publishing " + str(len(pages)) + " managed Wiki pages.")
            publish(pages)
        else:
            target = ROOT / "dist" / "wiki-preview"
            write_pages(target, pages)
            print("Generated " + str(len(pages)) + " preview pages: " + str(target))
            print("No remote changes. Run with --publish to upload Wiki.")
        return 0
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print("Wiki sync failed: " + str(error), file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
