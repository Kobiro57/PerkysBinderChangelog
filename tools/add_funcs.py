import sys

with open("src/MainWindowUI.cpp", "r") as f:
    content = f.read()

funcs = """
void MainWindowUI::SetCrosshairSwitcher(CrosshairSwitcher* switcher) {
    crosshairUI_.SetCrosshairSwitcher(switcher);
}

void MainWindowUI::SetTextureManager(TextureManager* manager) {
    hitmarkerUI_.SetTextureManager(manager);
    crosshairUI_.SetTextureManager(manager);
}

void MainWindowUI::SetDeviceGetter(std::function<IDirect3DDevice9*()> getter) {
    hitmarkerUI_.SetDeviceGetter(getter);
    crosshairUI_.SetDeviceGetter(getter);
}
"""

if "SetTextureManager" not in content:
    # insert before } // namespace keybinder at the end
    idx = content.rfind("} // namespace keybinder")
    if idx != -1:
        new_content = content[:idx] + funcs + "\n" + content[idx:]
        with open("src/MainWindowUI.cpp", "w") as f:
            f.write(new_content)
        print("Added functions")
    else:
        print("Namespace not found at end")
else:
    print("Functions already there")
