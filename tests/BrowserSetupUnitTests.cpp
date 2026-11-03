#include "../tools/BrowserSetup.cpp"
#include <cstdio>
int main() {
    int checks=0,failures=0;
    auto verify=[&](bool value){++checks;if(!value)++failures;};
    for(const std::string name:{"NLBinderBrowser.exe","locales/de.pak","CEF-LICENSE.txt","resources.pak"})verify(SafeEntry(name));
    for(const std::string name:{"","../outside.dll","locales/../../outside.dll","/outside.dll","C:/outside.dll",
        "locales\\de.pak","locales//de.pak","./libcef.dll","folder./file.dll","folder /file.dll","locales/",
        "libcef.dll:extra","//network/file"})verify(!SafeEntry(name));
    verify(AllowedHost(L"github.com"));verify(AllowedHost(L"release-assets.githubusercontent.com"));
    verify(!AllowedHost(L"github.com.evil.example"));verify(!AllowedHost(L"example.com"));verify(!AllowedHost(L"localhost"));
    Sha256 hash;hash.Add("abc",3);verify(hash.Finish()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    std::set<std::string> names;
    for(const auto& entry:cfg::kRuntimeFiles){verify(SafeEntry(entry.name));verify(names.insert(entry.name).second);verify(entry.bytes>0&&strlen(entry.sha256)==64);}
    verify(names.count("libcef.dll")&&names.count("NLBinderBrowser.exe")&&names.count("NLBinderBrowser.dll"));
    std::printf("BrowserSetupUnitTests: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
