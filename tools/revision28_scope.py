"""Exact source-removal scope for the authorized r28 locator removal."""
from pathlib import Path
import re

REMOVED_LOCATE_FILES = ('include/PoliceLocateTracker.h', 'include/ServerCheckpointRPCAdapter.h', 'include/CheckpointLocation.h', 'src/PoliceLocateBridge.cpp', 'src/PoliceLocateTracker.cpp', 'src/ServerCheckpointRPCAdapter.cpp', 'tests/PoliceLocateTrackerTests.cpp', 'tests/ServerCheckpointRPCAdapterTests.cpp', 'tests/CheckpointLocationTests.cpp', 'tests/PoliceLocateBridgeTests.cpp')

def assert_locator_removed(root):
    root=Path(root).resolve(strict=True)
    for relative in REMOVED_LOCATE_FILES:
        path=root/relative
        if path.exists() or path.is_symlink():
            raise ValueError('Removed locator source is still present: '+relative)
    names=re.compile(r'PoliceLocate|ServerCheckpointRPCAdapter|CheckpointLocation|\[ORTUNG\]')
    for directory,suffix in [('src','*.cpp'),('include','*.h')]:
        for path in (root/directory).glob(suffix):
            if names.search(path.read_text(encoding='utf-8-sig')):
                raise ValueError('Live production reference to removed locator: '+path.name)
    for relative in ['Keybinder.vcxproj','tools/run_tests.bat','tests/KeybindTests.cpp']:
        if names.search((root/relative).read_text(encoding='utf-8-sig')):
            raise ValueError('Build still includes removed locator: '+relative)
