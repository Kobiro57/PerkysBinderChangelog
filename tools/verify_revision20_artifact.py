"""Verify the r20 PE and embedded helpers without loading executable code."""
import hashlib,json
from pathlib import Path
import sys
from verify_revision18_artifact import PE, require

def main():
    root = Path(__file__).resolve().parents[1]
    asi = PE(sys.argv[1] if len(sys.argv) > 1 else root / "build/Release/NLBinder.asi")
    helper = PE(root / "build/ReconnectHelper.exe")
    require(asi.machine == helper.machine == 0x14C, "Expected both binaries to be x86")
    require(asi.characteristics & 0x2000, "ASI must be a DLL")
    require(not helper.characteristics & 0x2000, "Worker must be an executable")
    require(asi.resource(10, 300) == helper.data, "Embedded helper differs from built helper")
    setup = PE(root / "build/BrowserSetup.exe")
    require(setup.machine == 0x14C and not setup.characteristics & 0x2000, "Installer must be x86 EXE")
    require(asi.resource(10, 301) == setup.data, "Embedded browser installer differs")
    require(b"asInvoker" in setup.resource(24, 1), "Browser installer must not request elevation")
    require(asi.resource(10, 302) == (root / "third_party/miniz/LICENSE").read_bytes(), "Missing embedded miniz notice")
    imports = asi.imports()
    require("libcef.dll" not in [name.lower() for name in imports], "Chromium must remain out of the game process")
    require(not any(name.lower().startswith(("vcruntime", "msvcp", "ucrtbased")) for name in imports),
            "Unexpected external C++ runtime dependency")
    startup = (b"{33CCFF}[NLBinder]{FFFFFF} NLBinder v.0.1.2 by perkys & edited by KOBiRO "
               b"wurde {00CC00}erfolgreich{FFFFFF} gestartet!")
    require(startup in asi.data, "The requested v0.1.2 startup message is missing from the ASI")
    require(b"NLBinder v.0.1.2 by perkys & edited by KOBiRO" in asi.data,
            "The matching version/credit caption is missing")
    try:
        display_path = asi.path.resolve().relative_to(root).as_posix()
    except ValueError:
        display_path = asi.path.name
    result = {
        "revision": "v0.1.2-r20",
        "asi": display_path,
        "bytes": len(asi.data),
        "format": "PE32 x86 DLL",
        "sha256": hashlib.sha256(asi.data).hexdigest(),
        "imports": imports,
        "requested_startup_message_present": True,
        "version_and_credit": "NLBinder v.0.1.2 by perkys & edited by KOBiRO",
        "embedded_helper_id": 300,
        "embedded_helper_bytes": len(helper.data),
        "helper_sha256": hashlib.sha256(helper.data).hexdigest(),
        "embedded_helper_matches": True,
        "embedded_browser_setup_id": 301,
        "embedded_browser_setup_bytes": len(setup.data),
        "embedded_browser_setup_sha256": hashlib.sha256(setup.data).hexdigest(),
        "embedded_browser_setup_matches": True,
        "browser_setup_as_invoker": True,
    }
    report = json.dumps(result, indent=2)
    (root / "build/revision20-artifact.json").write_text(report + "\n", encoding="utf-8")
    print(report)


if __name__ == "__main__":
    main()
