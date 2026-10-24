// Harmless process fixture for the native reconnect-worker integration test.
// It never launches or locates the game or any installed launcher.
#include <windows.h>
#include <shellapi.h>
#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int count = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) return 10;
    if (count == 3 && std::wstring(arguments[1]) == L"--fixture-parent") {
        HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, arguments[2]);
        LocalFree(arguments);
        if (!event) return 11;
        const DWORD result = WaitForSingleObject(event, 30000);
        CloseHandle(event);
        return result == WAIT_OBJECT_0 ? 0 : 12;
    }

    wchar_t destination[32768] = {};
    const DWORD length = GetEnvironmentVariableW(L"NLBINDER_TEST_LAUNCH_LOG", destination, 32768);
    if (!length || length >= 32768) { LocalFree(arguments); return 13; }
    std::wstring output(1, L'\xFEFF');
    for (int index = 0; index < count; ++index) {
        output += arguments[index];
        output += L'\n';
    }
    LocalFree(arguments);
    HANDLE file = CreateFileW(destination, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return 14;
    DWORD written = 0;
    const DWORD size = static_cast<DWORD>(output.size() * sizeof(wchar_t));
    const bool success = WriteFile(file, output.data(), size, &written, nullptr) && written == size;
    CloseHandle(file);
    return success ? 0 : 15;
}
