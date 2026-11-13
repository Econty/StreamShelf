#!/usr/bin/env python3
"""Check that a release XPI contains the current Firefox extension source.

Checks the archive against the current source, verifies SHA-256 digests, and
uses Windows SignedCms/X509Chain to verify Mozilla's production signature.
No archive contents are extracted and no certificate stores are modified.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import hmac
import json
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

MAX_ARCHIVE_BYTES = 64 * 1024 * 1024
MAX_MEMBER_BYTES = 16 * 1024 * 1024
MAX_EXPANDED_BYTES = 64 * 1024 * 1024
EXPECTED_ADDON_ID = "connector@streamshelf.local"
SIGNATURE_MEMBERS = {"META-INF/manifest.mf", "META-INF/mozilla.sf", "META-INF/mozilla.rsa"}


def jar_sections(content: bytes) -> list[dict[str, str]]:
    """Parse JAR-style manifest headers, including folded continuation lines."""
    sections: list[dict[str, str]] = []
    fields: dict[str, str] = {}
    previous = None
    for line in content.decode("utf-8").splitlines():
        if not line:
            if fields:
                sections.append(fields)
                fields = {}
            previous = None
        elif line.startswith(" "):
            if previous is None:
                raise ValueError("Invalid manifest continuation")
            fields[previous] += line[1:]
        else:
            key, separator, value = line.partition(": ")
            key = key.lower()
            if not separator or not key or key in fields:
                raise ValueError("Invalid or duplicate signing manifest header")
            fields[key] = value
            previous = key
    if fields:
        sections.append(fields)
    return sections


def check_digest(expected: str, content: bytes) -> bool:
    try:
        decoded = base64.b64decode(expected, validate=True)
    except (ValueError, TypeError):
        return False
    return hmac.compare_digest(decoded, hashlib.sha256(content).digest())


def verify_signature(package: Path) -> None:
    powershell = shutil.which("pwsh") or shutil.which("powershell.exe") or shutil.which("powershell")
    if not powershell:
        raise ValueError("PowerShell is required to verify Mozilla's cryptographic signature")
    checker = Path(__file__).resolve().with_name("Verify-FirefoxCms.ps1")
    try:
        result = subprocess.run(
            [powershell, "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", str(checker),
             "-Package", str(package.resolve()), "-ExpectedId", EXPECTED_ADDON_ID],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30, check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ValueError(f"Could not verify Mozilla's signing certificate: {exc}") from exc
    if result.returncode:
        raise ValueError("Firefox package cryptographic signature or Mozilla certificate chain is invalid")


def validate_package(repo: Path, package: Path) -> None:
    shared = repo / "extension" / "shared"
    development = repo / "extension" / "firefox-development"
    manifest_path = development / "manifest.json"
    if not manifest_path.is_file() or not shared.is_dir():
        raise ValueError("Firefox development sources are missing")
    expected_manifest = manifest_path.read_bytes()
    manifest = json.loads(expected_manifest)
    version = manifest.get("version")
    addon_id = manifest.get("browser_specific_settings", {}).get("gecko", {}).get("id")
    if not isinstance(version, str) or not version or addon_id != EXPECTED_ADDON_ID:
        raise ValueError("Firefox development manifest has an unexpected version or add-on ID")
    if package.name != f"StreamShelf-Connector-{version}.xpi":
        raise ValueError(f"Expected StreamShelf-Connector-{version}.xpi")
    if not package.is_file():
        raise ValueError(f"Signed Firefox package is missing: {package}")
    if package.stat().st_size > MAX_ARCHIVE_BYTES:
        raise ValueError("Firefox package is too large")

    source = {p.relative_to(shared).as_posix(): p for p in shared.rglob("*") if p.is_file()}
    source["manifest.json"] = manifest_path
    with zipfile.ZipFile(package) as archive:
        members = archive.infolist()
        names = [member.filename for member in members]
        if len(names) != len(set(names)):
            raise ValueError("Firefox package contains duplicate entries")
        for name in names:
            parts = name.rstrip("/").split("/")
            if not name or name.startswith("/") or "\\" in name or any(part in ("", ".", "..") or ":" in part for part in parts):
                raise ValueError(f"Firefox package contains an unsafe entry name: {name!r}")
        if sum(member.file_size for member in members) > MAX_EXPANDED_BYTES:
            raise ValueError("Firefox package expands beyond the allowed limit")
        if any(member.file_size > MAX_MEMBER_BYTES for member in members):
            raise ValueError("Firefox package contains an oversized file")
        files = {member.filename for member in members if not member.is_dir()}
        if not SIGNATURE_MEMBERS.issubset(files):
            raise ValueError("Firefox package is missing the Mozilla signing envelope")
        extras = files - set(source) - {name for name in files if name.startswith("META-INF/")}
        if extras:
            raise ValueError(f"Firefox package contains unexpected files: {', '.join(sorted(extras))}")
        missing = set(source) - files
        if missing:
            raise ValueError(f"Firefox package is missing source files: {', '.join(sorted(missing))}")
        for name, path in source.items():
            if path.stat().st_size > MAX_MEMBER_BYTES or archive.read(name) != path.read_bytes():
                raise ValueError(f"Firefox package is stale: {name} differs from current source")
        for name in SIGNATURE_MEMBERS:
            if not archive.read(name):
                raise ValueError(f"Firefox package has an empty signing entry: {name}")
        manifest_bytes = archive.read("META-INF/manifest.mf")
        manifest_sections = jar_sections(manifest_bytes)
        signature_sections = jar_sections(archive.read("META-INF/mozilla.sf"))
        if not manifest_sections or not signature_sections or not check_digest(
            signature_sections[0].get("sha256-digest-manifest", ""), manifest_bytes
        ):
            raise ValueError("Firefox package signing manifest digest does not match")
        signed: dict[str, str] = {}
        for section in manifest_sections[1:]:
            name = section.get("name", "")
            digest = section.get("sha256-digest", "")
            if not name or name in signed or not digest:
                raise ValueError("Firefox package has an invalid or duplicate signed entry")
            signed[name] = digest
        if set(signed) != files - SIGNATURE_MEMBERS:
            raise ValueError("Firefox package files do not match its signing manifest")
        for name, digest in signed.items():
            if not check_digest(digest, archive.read(name)):
                raise ValueError(f"Firefox package signed digest does not match: {name}")
    verify_signature(package)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--package", required=True, type=Path)
    args = parser.parse_args()
    try:
        validate_package(args.repo, args.package)
    except (OSError, UnicodeError, ValueError, AttributeError, RuntimeError, NotImplementedError, zipfile.BadZipFile) as exc:
        print(f"Firefox release package check failed: {exc}", file=sys.stderr)
        return 1
    print("Firefox package matches the current source and its Mozilla production signature is valid (offline revocation not checked).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
