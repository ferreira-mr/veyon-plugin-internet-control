#include <QSettings>
#include <QStringList>
#include <cstdio>
#include <windows.h>

int main() {
    printf("Testing QSettings registry write to HKLM...\n");
    QString key = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Google\\Chrome\\URLBlocklist";
    QSettings settings(key, QSettings::NativeFormat);
    printf("Status before: %d\n", (int)settings.status());
    settings.setValue("1", "youtube.com");
    settings.sync();
    printf("Status after: %d\n", (int)settings.status());

    // Also check HKCU
    QString keyU = "HKEY_CURRENT_USER\\Software\\Policies\\Google\\Chrome\\URLBlocklist";
    QSettings settingsU(keyU, QSettings::NativeFormat);
    settingsU.setValue("1", "youtube.com");
    settingsU.sync();
    printf("Status HKCU after: %d\n", (int)settingsU.status());

    return 0;
}
