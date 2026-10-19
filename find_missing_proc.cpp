#include <windows.h>
#include <cstdio>
#include <vector>
#include <string>

int main() {
    SetDllDirectoryA("C:\\Program Files\\Veyon");
    
    // Test LoadLibraryA directly
    HMODULE hMod = LoadLibraryExA("C:\\Projetos\\plugin-veyon\\build\\internetcontrol.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hMod) {
        DWORD err = GetLastError();
        printf("LoadLibraryExA failed with error code: %lu\n", err);
    } else {
        printf("LoadLibraryExA SUCCESS!\n");
        FreeLibrary(hMod);
        return 0;
    }

    // Now let's inspect the imports of internetcontrol.dll and test each one
    HANDLE hFile = CreateFileA("C:\\Projetos\\plugin-veyon\\build\\internetcontrol.dll", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Cannot open DLL file\n");
        return 1;
    }
    DWORD size = GetFileSize(hFile, NULL);
    std::vector<BYTE> data(size);
    DWORD read = 0;
    ReadFile(hFile, data.data(), size, &read, NULL);
    CloseHandle(hFile);

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)data.data();
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(data.data() + dos->e_lfanew);
    auto importDesc = (PIMAGE_IMPORT_DESCRIPTOR)(data.data() + 
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    // We need section header mapping from RVA to file offset
    auto section = IMAGE_FIRST_SECTION(nt);
    auto rvaToOffset = [&](DWORD rva) -> DWORD {
        for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
            if (rva >= section[i].VirtualAddress && rva < section[i].VirtualAddress + section[i].Misc.VirtualSize) {
                return (rva - section[i].VirtualAddress) + section[i].PointerToRawData;
            }
        }
        return 0;
    };

    DWORD impOffset = rvaToOffset(nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    auto desc = (PIMAGE_IMPORT_DESCRIPTOR)(data.data() + impOffset);

    while (desc->Name != 0) {
        const char* dllName = (const char*)(data.data() + rvaToOffset(desc->Name));
        printf("\nChecking imported DLL: %s\n", dllName);
        HMODULE hDll = LoadLibraryA(dllName);
        if (!hDll) {
            printf("  -> FAILED to load DLL: %s (Error: %lu)\n", dllName, GetLastError());
            desc++;
            continue;
        }

        DWORD thunkRva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
        auto thunk = (PIMAGE_THUNK_DATA)(data.data() + rvaToOffset(thunkRva));
        int missingCount = 0;
        while (thunk->u1.AddressOfData != 0) {
            if (!(thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                auto importByName = (PIMAGE_IMPORT_BY_NAME)(data.data() + rvaToOffset((DWORD)thunk->u1.AddressOfData));
                const char* funcName = (const char*)importByName->Name;
                FARPROC proc = GetProcAddress(hDll, funcName);
                if (!proc) {
                    printf("  -> MISSING PROC: %s in %s\n", funcName, dllName);
                    missingCount++;
                }
            }
            thunk++;
        }
        if (missingCount == 0) {
            printf("  -> All procedures found!\n");
        }
        desc++;
    }

    return 0;
}
