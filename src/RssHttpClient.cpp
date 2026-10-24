#include "../include/RssHttpClient.h"

#include <windows.h>
#include <winhttp.h>

#include <string>

namespace keybinder {

namespace {

#ifdef KEYBINDER_TESTING
RssHttpClient::TestFetchHandler g_testFetchHandler = nullptr;
#endif

class WinHttpHandle final {
public:
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET handle) : handle_(handle) {}

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    WinHttpHandle(WinHttpHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    WinHttpHandle& operator=(WinHttpHandle&& other) noexcept {
        if (this != &other) {
            Reset();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }

        return *this;
    }

    ~WinHttpHandle() {
        Reset();
    }

    HINTERNET Get() const {
        return handle_;
    }

    void Reset(HINTERNET handle = nullptr) {
        if (handle_ != nullptr) {
            WinHttpCloseHandle(handle_);
        }

        handle_ = handle;
    }

private:
    HINTERNET handle_ = nullptr;
};

bool Utf8ToWide(const std::string& input, std::wstring& output) {
    output.clear();

    if (input.empty()) {
        return true;
    }

    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.c_str(), -1, nullptr, 0);
    if (required <= 0) {
        return false;
    }

    std::wstring wideBuffer(static_cast<std::size_t>(required), L'\0');
    const int written = MultiByteToWideChar(CP_UTF8,
                                            MB_ERR_INVALID_CHARS,
                                            input.c_str(),
                                            -1,
                                            &wideBuffer[0],
                                            required);
    if (written <= 0) {
        return false;
    }

    output.assign(wideBuffer.c_str());
    return true;
}

bool WideToUtf8(const std::wstring& input, std::string& output) {
    output.clear();

    if (input.empty()) {
        return true;
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return false;
    }

    std::string utf8Buffer(static_cast<std::size_t>(required), '\0');
    const int written = WideCharToMultiByte(CP_UTF8,
                                            0,
                                            input.c_str(),
                                            -1,
                                            &utf8Buffer[0],
                                            required,
                                            nullptr,
                                            nullptr);
    if (written <= 0) {
        return false;
    }

    output.assign(utf8Buffer.c_str());
    return true;
}

bool QueryHeaderString(HINTERNET request, const wchar_t* headerName, std::string& outValue) {
    outValue.clear();

    DWORD bufferSize = 0;
    const BOOL firstQuery = WinHttpQueryHeaders(request,
                                                WINHTTP_QUERY_CUSTOM,
                                                const_cast<wchar_t*>(headerName),
                                                WINHTTP_NO_OUTPUT_BUFFER,
                                                &bufferSize,
                                                WINHTTP_NO_HEADER_INDEX);
    if (firstQuery == TRUE) {
        return true;
    }

    const DWORD error = GetLastError();
    if (error == ERROR_WINHTTP_HEADER_NOT_FOUND) {
        return true;
    }

    if (error != ERROR_INSUFFICIENT_BUFFER || bufferSize == 0) {
        return false;
    }

    std::wstring wideValue(bufferSize / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(request,
                             WINHTTP_QUERY_CUSTOM,
                             const_cast<wchar_t*>(headerName),
                             &wideValue[0],
                             &bufferSize,
                             WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }

    while (!wideValue.empty() && wideValue.back() == L'\0') {
        wideValue.pop_back();
    }

    return WideToUtf8(wideValue, outValue);
}

bool ReadResponseBody(HINTERNET request, std::string& outBody) {
    outBody.clear();

    char buffer[8192];
    DWORD bytesRead = 0;
    do {
        if (!WinHttpReadData(request, buffer, static_cast<DWORD>(sizeof(buffer)), &bytesRead)) {
            return false;
        }

        if (bytesRead > 0) {
            outBody.append(buffer, buffer + bytesRead);
        }
    } while (bytesRead > 0);

    return true;
}

void AppendHeaderLine(std::wstring& headers, const wchar_t* name, const std::wstring& value) {
    if (value.empty()) {
        return;
    }

    headers.append(name);
    headers.append(L": ");
    headers.append(value);
    headers.append(L"\r\n");
}

} // namespace

bool RssHttpClient::Fetch(const std::string& url,
                          std::string& outBody,
                          const std::string& ifModifiedSince,
                          const std::string& ifNoneMatch,
                          std::string& outLastModified,
                          std::string& outEtag,
                          unsigned long& outStatusCode) const {
#ifdef KEYBINDER_TESTING
    if (g_testFetchHandler != nullptr) {
        return g_testFetchHandler(url,
                                  outBody,
                                  ifModifiedSince,
                                  ifNoneMatch,
                                  outLastModified,
                                  outEtag,
                                  outStatusCode);
    }
#endif

    outBody.clear();
    outLastModified.clear();
    outEtag.clear();
    outStatusCode = 0;

    std::wstring wideUrl;
    std::wstring wideIfModifiedSince;
    std::wstring wideIfNoneMatch;
    if (!Utf8ToWide(url, wideUrl) ||
        !Utf8ToWide(ifModifiedSince, wideIfModifiedSince) ||
        !Utf8ToWide(ifNoneMatch, wideIfNoneMatch)) {
        return false;
    }

    URL_COMPONENTS components = {};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &components)) {
        return false;
    }

    const bool isHttps = components.nScheme == INTERNET_SCHEME_HTTPS;
    if (!isHttps && components.nScheme != INTERNET_SCHEME_HTTP) {
        return false;
    }

    std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path;
    if (components.dwUrlPathLength > 0 && components.lpszUrlPath != nullptr) {
        path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    }
    if (path.empty()) {
        path = L"/";
    }
    if (components.dwExtraInfoLength > 0 && components.lpszExtraInfo != nullptr) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }

    WinHttpHandle session(WinHttpOpen(L"NLBinder/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS,
                                      0));
    if (session.Get() == nullptr) {
        return false;
    }

    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    if (!WinHttpSetOption(session.Get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols))) {
        return false;
    }

    if (!WinHttpSetTimeouts(session.Get(), 5000, 5000, 5000, 10000)) {
        return false;
    }

    WinHttpHandle connection(WinHttpConnect(session.Get(), host.c_str(), components.nPort, 0));
    if (connection.Get() == nullptr) {
        return false;
    }

    const DWORD requestFlags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle request(WinHttpOpenRequest(connection.Get(),
                                            L"GET",
                                            path.c_str(),
                                            nullptr,
                                            WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            requestFlags));
    if (request.Get() == nullptr) {
        return false;
    }

    std::wstring requestHeaders;
    AppendHeaderLine(requestHeaders, L"If-Modified-Since", wideIfModifiedSince);
    AppendHeaderLine(requestHeaders, L"If-None-Match", wideIfNoneMatch);

    const wchar_t* additionalHeaders = requestHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : requestHeaders.c_str();
    const DWORD additionalHeadersLength = requestHeaders.empty() ? 0 : static_cast<DWORD>(requestHeaders.size());

    if (!WinHttpSendRequest(request.Get(),
                            additionalHeaders,
                            additionalHeadersLength,
                            WINHTTP_NO_REQUEST_DATA,
                            0,
                            0,
                            0)) {
        return false;
    }

    if (!WinHttpReceiveResponse(request.Get(), nullptr)) {
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(request.Get(),
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,
                             &statusCode,
                             &statusCodeSize,
                             WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }

    outStatusCode = statusCode;

    if (!QueryHeaderString(request.Get(), L"Last-Modified", outLastModified) ||
        !QueryHeaderString(request.Get(), L"ETag", outEtag)) {
        return false;
    }

    if (statusCode == HTTP_STATUS_NOT_MODIFIED) {
        return true;
    }

    if (statusCode < HTTP_STATUS_OK || statusCode >= 300) {
        return false;
    }

    return ReadResponseBody(request.Get(), outBody);
}

#ifdef KEYBINDER_TESTING
void RssHttpClient::SetTestFetchHandler(TestFetchHandler handler) {
    g_testFetchHandler = handler;
}

void RssHttpClient::ResetTestFetchHandler() {
    g_testFetchHandler = nullptr;
}
#endif

} // namespace keybinder
