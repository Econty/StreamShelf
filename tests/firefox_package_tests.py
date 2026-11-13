"""Regression checks for release package/source and Mozilla signature validation."""
import base64
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECK = ROOT / "scripts" / "Verify-FirefoxPackage.py"
MANIFEST = ROOT / "extension" / "firefox-development" / "manifest.json"
SHARED = ROOT / "extension" / "shared"
VERSION = json.loads(MANIFEST.read_text(encoding="utf-8"))["version"]


class FirefoxPackageTests(unittest.TestCase):
    def check(self, path):
        return subprocess.run([sys.executable, str(CHECK), "--repo", str(ROOT), "--package", str(path)], capture_output=True, text=True)

    def create(self, directory, *, changed=None, extra=None, signed=True):
        path = Path(directory) / f"StreamShelf-Connector-{VERSION}.xpi"
        with zipfile.ZipFile(path, "w") as archive:
            contents = {"manifest.json": MANIFEST.read_bytes()}
            for source in SHARED.rglob("*"):
                if source.is_file():
                    name = source.relative_to(SHARED).as_posix()
                    data = source.read_bytes()
                    contents[name] = b"stale extension" if changed == name else data
            for name, data in contents.items():
                archive.writestr(name, data)
            if signed:
                # Use accurate hashes to ensure the deliberately fake CMS is
                # rejected by the cryptographic check, not by the hash check.
                digest = lambda data: base64.b64encode(hashlib.sha256(data).digest()).decode("ascii")
                manifest = ("Manifest-Version: 1.0\r\n\r\n" + "".join(
                    f"Name: {name}\r\nSHA256-Digest: {digest(data)}\r\n\r\n"
                    for name, data in contents.items()
                )).encode("utf-8")
                signature = f"Signature-Version: 1.0\r\nSHA256-Digest-Manifest: {digest(manifest)}\r\n\r\n".encode("ascii")
                archive.writestr("META-INF/manifest.mf", manifest)
                archive.writestr("META-INF/mozilla.sf", signature)
                archive.writestr("META-INF/mozilla.rsa", b"not a real Mozilla signature")
            if extra:
                archive.writestr(extra, b"unexpected")
        return path

    def test_fake_signing_envelope_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.check(self.create(directory))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("cryptographic signature", result.stderr)

    def test_genuine_mozilla_signature_matches_its_source(self):
        old = ROOT / "extension" / "firefox-signed" / "StreamShelf-Connector-1.0.4.xpi"
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory) / "repo"
            shared = repo / "extension" / "shared"
            development = repo / "extension" / "firefox-development"
            shared.mkdir(parents=True)
            development.mkdir(parents=True)
            with zipfile.ZipFile(old) as archive:
                for name in archive.namelist():
                    if name.startswith("META-INF/") or name.endswith("/"):
                        continue
                    target = (development if name == "manifest.json" else shared) / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(archive.read(name))
            result = subprocess.run([sys.executable, str(CHECK), "--repo", str(repo), "--package", str(old)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("Mozilla production signature is valid", result.stdout)

    def test_tampering_is_rejected_even_if_source_matches(self):
        old = ROOT / "extension" / "firefox-signed" / "StreamShelf-Connector-1.0.4.xpi"
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory) / "repo"
            shared = repo / "extension" / "shared"
            development = repo / "extension" / "firefox-development"
            shared.mkdir(parents=True)
            development.mkdir(parents=True)
            tampered = Path(directory) / old.name
            with zipfile.ZipFile(old) as source, zipfile.ZipFile(tampered, "w") as output:
                for entry in source.infolist():
                    data = source.read(entry.filename)
                    if entry.filename == "background.js":
                        data += b"\n// modified after signing\n"
                    output.writestr(entry, data)
                    if entry.filename.startswith("META-INF/") or entry.is_dir():
                        continue
                    target = (development if entry.filename == "manifest.json" else shared) / entry.filename
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(data)
            result = subprocess.run([sys.executable, str(CHECK), "--repo", str(repo), "--package", str(tampered)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("signed digest does not match", result.stderr)

    def test_missing_or_stale_package_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / f"StreamShelf-Connector-{VERSION}.xpi"
            self.assertNotEqual(self.check(missing).returncode, 0)
            stale = self.check(self.create(directory, changed="background.js"))
            self.assertNotEqual(stale.returncode, 0)
            self.assertIn("background.js", stale.stderr)

    def test_unsigned_and_unexpected_files_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertNotEqual(self.check(self.create(directory, signed=False)).returncode, 0)
            self.assertNotEqual(self.check(self.create(directory, extra="injected.js")).returncode, 0)
            self.assertNotEqual(self.check(self.create(directory, extra="META-INF/../../injected.js")).returncode, 0)

    def test_older_signed_package_cannot_be_renamed_as_current(self):
        with tempfile.TemporaryDirectory() as directory:
            old = ROOT / "extension" / "firefox-signed" / "StreamShelf-Connector-1.0.4.xpi"
            renamed = Path(directory) / f"StreamShelf-Connector-{VERSION}.xpi"
            renamed.write_bytes(old.read_bytes())
            self.assertNotEqual(self.check(renamed).returncode, 0)


if __name__ == "__main__":
    unittest.main()
