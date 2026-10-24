import re

with open("src/MainWindowUI.cpp", "r") as f:
    content = f.read()

match = re.search(
    r'UIComponents::BeginCard\("SystemCard"\);.*?UIComponents::EndCard\(\);',
    content,
    re.DOTALL,
)
if match:
    old_card = match.group(0)
    new_card = """UIComponents::BeginCard("SystemCard");
    UIComponents::CardHeader("System & Erweitert");

    bool dummyBool = false;
    UIComponents::Toggle("Auto-hudscalefix beim Start", &settingsManager_->GetMutable().auto_hudscalefix);
    ImGui::Spacing();

    UIComponents::EndCard();"""
    content = content.replace(old_card, new_card)
    with open("src/MainWindowUI.cpp", "w") as f:
        f.write(content)
    print("Fixed SystemCard")
else:
    print("Not found")
