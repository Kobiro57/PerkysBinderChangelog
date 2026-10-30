"""Export a verified r15 source snapshot, current ASI and complete changelog.

Reads Git blobs without changing Git or the game installation. The destination
must not exist. Historical release binaries, Git metadata, local build outputs
and private runtime files are excluded. The proof manifest stays under build/.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import re
import subprocess


ROOT = Path(__file__).resolve().parents[1]
RELEASE = "v0.1.1-r15"
PACKAGE = "dist/NLBinder-0.1.1-r15"
PRIVATE_PARTS = {
    ".git", ".vs", ".aws", ".codex", ".agents", "__pycache__", "build",
    "output", "debug", "release", "obj", "cmakefiles", ".venv", "venv",
}
PRIVATE_NAMES = {
    "keybinder.json", "jobs_data.json", "imgui.ini", ".env", ".git-credentials",
    "credentials", "id_rsa", "id_ed25519", "nul",
}
PRIVATE_SUFFIXES = {".log", ".obj", ".pdb", ".ilk", ".exp", ".pyc", ".suo", ".user", ".o"}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(repo, *args):
    return subprocess.check_output(["git", *args], cwd=repo)


def safe_source_path(relative):
    path = PurePosixPath(relative)
    require(path.parts and not path.is_absolute() and ".." not in path.parts,
            "Unsafe source path: " + relative)
    require("\\" not in relative and ":" not in relative, "Invalid source path: " + relative)
    require(not any(part.lower() in PRIVATE_PARTS for part in path.parts[:-1]),
            "Private/build directory in source snapshot: " + relative)
    require(path.name.lower() not in PRIVATE_NAMES and path.suffix.lower() not in PRIVATE_SUFFIXES,
            "Private/runtime file in source snapshot: " + relative)


def source_entries(repo, commit):
    entries = {}
    for entry in git(repo, "ls-tree", "-r", "-z", commit).split(b"\0"):
        if not entry:
            continue
        metadata, raw_path = entry.split(b"\t", 1)
        mode, kind, oid = metadata.split()
        relative = raw_path.decode("utf-8")
        require(kind == b"blob" and mode in {b"100644", b"100755"},
                "Unsupported symlink/submodule: " + relative)
        entries[relative] = oid
    return entries


def read_blobs(repo, entries):
    oids = sorted(set(entries.values()))
    process = subprocess.Popen(["git", "cat-file", "--batch"], cwd=repo,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, errors = process.communicate(b"\n".join(oids) + b"\n")
    require(process.returncode == 0, "Cannot read Git source objects: " + errors.decode("utf-8", "replace"))
    offset, blobs = 0, {}
    for oid in oids:
        end_header = output.find(b"\n", offset)
        require(end_header >= offset, "Truncated Git source header")
        header = output[offset:end_header].split()
        require(len(header) == 3 and header[0] == oid and header[1] == b"blob", "Unexpected source object")
        size = int(header[2])
        start, end = end_header + 1, end_header + 1 + size
        require(output[end:end + 1] == b"\n", "Truncated source file")
        blobs[oid] = output[start:end]
        offset = end + 1
    require(offset == len(output), "Unexpected trailing Git source data")
    return {relative: blobs[oid] for relative, oid in entries.items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", required=True, type=Path)
    parser.add_argument("--repository", type=Path, default=ROOT / "build/github-publication/repository")
    parser.add_argument("--revision", default="HEAD")
    parser.add_argument("--binary", type=Path, default=ROOT / "build/Release/NLBinder.asi")
    parser.add_argument("--manifest", type=Path, default=ROOT / "build/source-export-r15.json")
    args = parser.parse_args()
    repo = args.repository.resolve(strict=True)
    destination = args.destination.resolve()
    manifest_path = args.manifest.resolve()
    require(not destination.exists(), "Destination already exists; nothing will be overwritten: " + str(destination))
    require(destination.parent.is_dir(), "Destination parent must already exist")
    require(not destination.is_relative_to(repo), "Export must be outside the publication checkout")
    require(not manifest_path.is_relative_to(destination), "Manifest must remain outside the source export")
    commit = git(repo, "rev-parse", "--verify", "--end-of-options", args.revision + "^{commit}").decode().strip()
    require(re.fullmatch("[0-9a-f]{40}", commit), "Unexpected Git revision")
    entries = source_entries(repo, commit)
    selected = {}
    for relative, oid in entries.items():
        if PurePosixPath(relative).parts[0] == "dist":
            continue  # Prior ASIs/ZIPs are available through the Git history.
        safe_source_path(relative)
        selected[relative] = oid
    extras = {
        "NLBinder.asi": PACKAGE + "/NLBinder.asi",
        "CHANGELOG-GESAMT.txt": "docs/CHANGELOG-GESAMT.txt",
        "README.md": "docs/BUILD-README.md",
        "LIESMICH-DE.md": PACKAGE + "/LIESMICH.md",
        "BUILD-INFO.txt": PACKAGE + "/BUILD-INFO.txt",
        "PRUEFBERICHT.json": PACKAGE + "/PRUEFBERICHT.json",
        "SHA256SUMS.txt": PACKAGE + "/SHA256SUMS.txt",
        "RECONNECT-TEST.json": PACKAGE + "/RECONNECT-TEST.json",
    }
    for source in extras.values():
        require(source in entries, "Verified r15 publication file missing: " + source)
    read_entries = dict(selected)
    read_entries.update({source: entries[source] for source in extras.values()})
    files = read_blobs(repo, read_entries)
    exported = {relative: files[relative] for relative in selected}
    for target, source in extras.items():
        exported[target] = files[source]
    binary = exported["NLBinder.asi"]
    require(binary == args.binary.read_bytes(), "Published ASI differs from the locally built NLBinder.asi")
    report = json.loads(exported["PRUEFBERICHT.json"].decode("utf-8-sig"))
    require(report.get("revision") == RELEASE and report.get("asi") == "NLBinder.asi" and report.get("sha256") == digest(binary)
            and report.get("bytes") == len(binary) and report.get("embedded_helper_matches") is True,
            "r15 artifact verification is missing or stale")
    require(exported["Keybinder.vcxproj"].count(b"<TargetName>NLBinder</TargetName>") == 2,
            "Release/Debug project names are not both NLBinder")
    folded = [relative.casefold() for relative in exported]
    require(len(folded) == len(set(folded)), "Case-insensitive destination path collision")
    require(git(repo, "rev-parse", "--verify", "--end-of-options", args.revision + "^{commit}").decode().strip() == commit,
            "Source revision changed during snapshot reading")
    # All input and publication checks finish before the first destination write.
    destination.mkdir()
    for relative, data in sorted(exported.items()):
        target = destination / relative
        require(target.resolve().is_relative_to(destination), "Export escaped the destination")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    actual = {path.relative_to(destination).as_posix(): path for path in destination.rglob("*") if path.is_file()}
    require(set(actual) == set(exported), "Exported file set differs")
    for relative, data in exported.items():
        require(actual[relative].read_bytes() == data, "Exported file differs: " + relative)
        require(not actual[relative].is_symlink(), "Unexpected exported symlink: " + relative)
    manifest = {
        "release": RELEASE, "source_commit": commit, "destination": str(destination),
        "exported_files": len(exported), "exported_bytes": sum(map(len, exported.values())),
        "source_files": len(selected), "all_files_verified": True,
        "historical_release_binaries_in_export": 0, "private_runtime_files": 0,
        "asi_bytes": len(binary), "asi_sha256": digest(binary),
        "changelog_sha256": digest(exported["CHANGELOG-GESAMT.txt"]),
        "root_file_sources": extras,
        "files": {relative: digest(data) for relative, data in sorted(exported.items())},
    }
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in manifest.items() if key != "files"}, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
