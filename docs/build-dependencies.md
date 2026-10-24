# Build dependencies

The Win32 Release build uses Visual Studio 2022 Build Tools (v143) and Windows SDK 10.0. The build scripts locate the installed C++ toolchain with Visual Studio Installer's `vswhere.exe`; no fixed Visual Studio installation directory is required.

The following sources are restored inside this project:

| Dependency | Source | Revision |
| --- | --- | --- |
| Plugin-SDK | https://github.com/DK22Pac/plugin-sdk | `15f15b60bbf74c106e1b496ff92c98764abf4605` |
| RakHook | https://github.com/imring/RakHook | `109daae3031cd030d272fa35407412443f03dede` (the existing 1.0 API) |

RakHook 2.0 changed its public interface. The binder deliberately uses the earlier API that its network adapters were written against. The existing local `cyanide` and `polyhook2` adapters continue to use the Plugin-SDK SafetyHook implementation.

Plugin-SDK includes the required DirectX 9 headers and import libraries. Build its static library with `tools\build_plugin_sdk.bat`. That script generates the SDK's Visual Studio project with its bundled Premake if necessary. `tools\build_release.bat` performs this step automatically when `plugin.lib` is missing.

The runtime remains targeted at GTA San Andreas 1.0 US with SA-MP 0.3.DL-R1. Restoring these build dependencies does not broaden the supported game versions.

To build the plugin and run its standalone test suites:

```bat
tools\build_and_test.bat
```

The output plugin is `build\Release\Keybinder.asi`.
