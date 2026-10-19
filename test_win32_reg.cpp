#include <windows.h>
#include <cstdio>
#include <string>

int main() {
    HKEY hKey;
    LONG res = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Policies\\Google\\Chrome\\URLBlocklist",
                               0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
    printf("RegCreateKeyExW HKCU result: %ld\n", res);
    if (res == ERROR_SUCCESS) {
        const wchar_t* val = L"youtube.com";
        res = RegSetValueExW(hKey, L"1", 0, REG_SZ, (const BYTE*)val, (wcslen(val) + 1) * sizeof(wchar_t));
        printf("RegSetValueExW result: %ld\n", res);
        RegCloseKey(hKey);
    }
    return 0;
}
