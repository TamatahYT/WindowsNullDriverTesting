#include <windows.h>
#include <stdio.h>

#define DEVICE_NAME L"\\\\.\\GLOBALROOT\\Device\\Null"

void TestLargeWriteOperations() {
    HANDLE hDevice = CreateFileW(DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open device (Error: %d)\n", GetLastError());
        return;
    }

    printf("[*] Testing large buffer writes to null device\n\n");

    // Test increasingly large buffers
    SIZE_T sizes[] = {
        0x1000,       // 4KB
        0x10000,      // 64KB
        0x100000,     // 1MB
        0x1000000,    // 16MB
        0x7FFFFFFF    // ~2GB (maximum reasonable size)
    };

    for (int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        printf("[+] Testing write size: 0x%zX bytes\n", sizes[i]);

        // Allocate and fill buffer with pattern
        PBYTE buffer = (PBYTE)VirtualAlloc(NULL, sizes[i], MEM_COMMIT, PAGE_READWRITE);
        if (!buffer) {
            printf("[-] Failed to allocate 0x%zX bytes (Error: %d)\n", sizes[i], GetLastError());
            continue;
        }

        // Fill with pattern to detect corruption
        memset(buffer, 0x41 + i, sizes[i]);  // Fills with 'A', 'B', etc.

        DWORD bytesWritten = 0;
        BOOL result = WriteFile(hDevice, buffer, (DWORD)sizes[i], &bytesWritten, NULL);

        if (result) {
            printf("[+] Write succeeded. Bytes written: %lu\n", bytesWritten);
        }
        else {
            printf("[-] Write failed (Error: %d)\n", GetLastError());
        }

        // Verify buffer wasn't corrupted
        for (SIZE_T j = 0; j < min(sizes[i], 0x100); j++) {
            if (buffer[j] != (0x41 + i)) {
                printf("[!] Buffer corruption detected at offset 0x%zX!\n", j);
                break;
            }
        }

        VirtualFree(buffer, 0, MEM_RELEASE);
    }

    CloseHandle(hDevice);
}

void TestMassiveWrite() {
    HANDLE hDevice = CreateFileW(DEVICE_NAME,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,  // Bypass cache
        NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open device (Error: %d)\n", GetLastError());
        return;
    }

    printf("\n[*] Testing massive write (4GB)\n");

    // Use chunked writing to test extremely large transfers
    const SIZE_T chunkSize = 0x100000;  // 1MB chunks
    const SIZE_T totalSize = 0x100000000; // 4GB total
    SIZE_T remaining = totalSize;

    PBYTE buffer = (PBYTE)VirtualAlloc(NULL, chunkSize, MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) {
        printf("[-] Failed to allocate buffer\n");
        CloseHandle(hDevice);
        return;
    }

    memset(buffer, 0xCC, chunkSize);  // Fill with pattern

    while (remaining > 0) {
        DWORD thisWrite = (DWORD)min(chunkSize, remaining);
        DWORD bytesWritten = 0;

        if (!WriteFile(hDevice, buffer, thisWrite, &bytesWritten, NULL)) {
            printf("[-] Write failed at offset 0x%zX (Error: %d)\n",
                totalSize - remaining, GetLastError());
            break;
        }

        if (bytesWritten != thisWrite) {
            printf("[~] Partial write at offset 0x%zX (%lu of %lu bytes)\n",
                totalSize - remaining, bytesWritten, thisWrite);
        }

        remaining -= bytesWritten;
    }

    printf("[+] Massive write test completed\n");
    VirtualFree(buffer, 0, MEM_RELEASE);
    CloseHandle(hDevice);
}

int main() {
    printf("[*] Starting Large Buffer Write Tests\n\n");

    TestLargeWriteOperations();
    TestMassiveWrite();

    printf("\n[*] Testing complete. Monitor system stability.\n");
    return 0;
}