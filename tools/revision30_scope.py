"""Current v0.1.3 spray-navigation source scope; the r28 locator stays removed."""
from revision28_scope import REMOVED_LOCATE_FILES, assert_locator_removed

SPRAY_SOURCE_FILES = (
    'include/WaypointTour.h', 'src/WaypointTour.cpp', 'tests/WaypointTourTests.cpp',
    'include/SprayAccess.h', 'tests/SprayAccessTests.cpp',
    'include/SprayCatalog.h', 'src/SprayCatalog.cpp', 'tests/SprayCatalogTests.cpp',
    'include/SpraySpots.h', 'include/SpraysNavigator.h', 'src/SpraysNavigator.cpp',
    'tests/SpraysNavigatorTests.cpp', 'tests/GpsTemporaryTests.cpp',
    'include/GpsManager.h', 'src/GpsManager.cpp', 'src/Main.cpp',
    'Keybinder.vcxproj', 'tools/run_tests.bat', 'tests/KeybindTests.cpp',
)

def assert_sprays_sources(root):
    assert_locator_removed(root)
    for relative in SPRAY_SOURCE_FILES:
        path=root/relative
        if not path.is_file() or path.is_symlink():
            raise ValueError('Spray navigation source missing or redirected: '+relative)
