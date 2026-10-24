"""Prepare auditable source snapshots; never reads credentials or pushes Git.

The original ZIP is read only. Personal runtime files, nested Git metadata and
compiler output are excluded. The current ASI is copied without modification.
"""
from pathlib import Path, PurePosixPath
import hashlib
import json
import os
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = Path.home() / "Downloads/NLBinder-master.zip"
OUTPUT = ROOT / "build/github-publication"
BASELINE = OUTPUT / "original"
CURRENT = OUTPUT / "current"
EXPECTED_ORIGINAL = "a328e78eea63420e9bfe9c084883100177fe0fd9c2d62c4fcaef38c0a44801e9"
EXPECTED_ASI = "fc27a8a39fc2f78c40f1b686d5a71dcc0c2956854d1250278a84bd79200c7cef"
SOURCE_DIRS = {"src", "include", "res", "tests", "tools", "third_party", "docs"}
ROOT_FILES = {".gitignore", "README.md", "Keybinder.rc", "Keybinder.sln", "Keybinder.vcxproj"}
EXCLUDED_DIRS = {".git", ".vs", "__pycache__", "output", "debug", "release", "obj", "cmakefiles"}
EXCLUDED_SUFFIXES = {".obj", ".pdb", ".ilk", ".exp", ".suo", ".user", ".log", ".pyc", ".o"}
EXCLUDED_NAMES = {"keybinder.json", "jobs_data.json", "imgui.ini", "nul", "keybinder_crash.log"}
VENDOR_BINARY_SUFFIXES = {".lib", ".exe", ".dll"}
RELEASE_DIR = Path("dist/NLBinder-0.1.1-r3")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def allowed(path):
    path = PurePosixPath(path)
    if path.is_absolute() or ".." in path.parts or not path.parts:
        return False
    if path.parts[0] not in SOURCE_DIRS and str(path) not in ROOT_FILES:
        return False
    if any(part.lower() in EXCLUDED_DIRS for part in path.parts[:-1]):
        return False
    if path.name.lower() in EXCLUDED_NAMES or path.suffix.lower() in EXCLUDED_SUFFIXES:
        return False
    if path.suffix.lower() in {".asi", ".exe", ".lib", ".dll"}:
        # Only pre-existing source dependency binaries belong in the tree.
        return path.parts[0] in {"include", "third_party"}
    return True


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def snapshot_files(directory):
    return {str(path.relative_to(directory)).replace("\\", "/"): path
            for path in directory.rglob("*") if path.is_file()}


def main():
    assert sha(ORIGINAL.read_bytes()) == EXPECTED_ORIGINAL, "Original ZIP differs"
    assert sha((ROOT / RELEASE_DIR / "Keybinder.asi").read_bytes()) == EXPECTED_ASI, "ASI differs"
    BASELINE.mkdir(parents=True, exist_ok=False)
    CURRENT.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(ORIGINAL) as archive:
        comment = archive.comment.decode("ascii", errors="strict").strip()
        for entry in archive.infolist():
            if entry.is_dir():
                continue
            path = PurePosixPath(entry.filename)
            if path.parts[0] != "NLBinder-master":
                raise ValueError("Unexpected archive root")
            relative = PurePosixPath(*path.parts[1:])
            if allowed(relative):
                write(BASELINE / str(relative), archive.read(entry))
    for start in SOURCE_DIRS:
        for current, directories, files in os.walk(ROOT / start):
            directories[:] = [name for name in directories if name.lower() not in EXCLUDED_DIRS]
            for name in files:
                path = Path(current) / name
                relative = path.relative_to(ROOT).as_posix()
                if path.is_symlink():
                    raise ValueError(f"Unexpected symlink: {relative}")
                if allowed(relative):
                    write(CURRENT / relative, path.read_bytes())
    for name in ROOT_FILES:
        write(CURRENT / name, (ROOT / name).read_bytes())
    for path in (ROOT / RELEASE_DIR).iterdir():
        if path.suffix.lower() in {".asi", ".md", ".json", ".txt"}:
            data = path.read_bytes()
            if path.name == "PRUEFBERICHT.json":
                report = json.loads(data)
                report["asi"] = "Keybinder.asi"  # Avoid publishing machine-specific profile paths.
                data = (json.dumps(report, indent=2) + "\n").encode()
            write(CURRENT / RELEASE_DIR / path.name, data)
    # Preserve the complete original project README when adding the review entry page.
    write(CURRENT / "docs/ORIGINAL-README.md", (BASELINE / "README.md").read_bytes())
    write(CURRENT / "docs/BUILD-README.md", (ROOT / "README.md").read_bytes())
    write(CURRENT / "README.md", (ROOT / "docs/review/README-GITHUB.md").read_bytes())
    write(CURRENT / ".gitattributes", b"* text=auto\n*.cpp text eol=lf\n*.h text eol=lf\n*.md text eol=lf\n*.json text eol=lf\n*.bat text eol=crlf\n*.asi binary\n*.png binary\n*.ttf binary\n*.lib binary\n*.exe binary\n*.dll binary\n")
    original_files = snapshot_files(BASELINE)
    current_files = snapshot_files(CURRENT)
    changes = []
    for relative in sorted(original_files.keys() | current_files.keys()):
        before = original_files[relative].read_bytes() if relative in original_files else None
        after = current_files[relative].read_bytes() if relative in current_files else None
        if before == after:
            continue
        # Distinguish formatting-only CRLF/LF differences from content changes.
        formatting_only = before is not None and after is not None and before.replace(b"\r\n", b"\n") == after.replace(b"\r\n", b"\n")
        changes.append({"path": relative, "status": "added" if before is None else "deleted" if after is None else "modified",
                        "dependency": relative.startswith(("include/plugin-sdk/", "include/RakHook/")),
                        "line_endings_only": formatting_only,
                        "before_sha256": sha(before) if before is not None else None,
                        "after_sha256": sha(after) if after is not None else None})
    record = {"original_archive": "NLBinder-master.zip", "original_sha256": EXPECTED_ORIGINAL,
              "original_archive_commit_comment": comment, "release_asi_sha256": EXPECTED_ASI,
              "baseline_files": len(original_files), "current_files": len(current_files), "changes": changes}
    write(CURRENT / "docs/review/SOURCE-COMPARISON.json", (json.dumps(record, indent=2, ensure_ascii=False) + "\n").encode())
    relevant = [item for item in changes if not item["dependency"] and not item["line_endings_only"]]
    text = ["# Dateivergleich mit dem Originalarchiv", "",
            f"Originalarchiv: `{EXPECTED_ORIGINAL}`; Commitkennung im ZIP: `{comment}`.", "",
            "Alle Dateien einschließlich wiederhergestellter Abhängigkeiten stehen in [SOURCE-COMPARISON.json](SOURCE-COMPARISON.json).",
            "Reine CRLF/LF-Unterschiede sind dort separat markiert. Diese Liste enthält Inhaltsänderungen außerhalb der beiden wiederhergestellten SDK-Verzeichnisse.", "",
            "| Status | Datei |", "| --- | --- |"]
    for item in relevant:
        path = item["path"]
        text.append(f"| {item['status']} | [{path}](../../{path}) |")
    write(CURRENT / "docs/review/DATEIVERGLEICH.md", ("\n".join(text) + "\n").encode())
    print(json.dumps({"original": str(BASELINE), "current": str(CURRENT), "original_files": len(original_files),
                      "current_files": len(current_files), "changed_or_added": len(changes),
                      "non_dependency_content_changes": len(relevant), "archive_comment": comment}, indent=2))


if __name__ == "__main__":
    main()
