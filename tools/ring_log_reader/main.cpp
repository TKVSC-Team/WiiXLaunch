// ring_log_reader - host-side debug console for WiiXLaunch's ring-buffer logger.
//
// The mod (compiled as PowerPC/big-endian code running inside Cemu) writes into
// a fixed-layout ring buffer (see include/wiixlaunch/debug_log.hpp) that lives
// as a plain static global inside its own codecave payload. Cemu emulates the
// Wii U's memory byte-for-byte (big-endian) inside its own host process, so we
// find the buffer by scanning Cemu.exe's committed memory for its magic cookie,
// then poll it and print new entries as they arrive. No debugger, no symbols,
// no cooperation from Cemu needed - just ReadProcessMemory.
//
// All multi-byte fields in the target buffer are big-endian (PowerPC), so every
// field is byte-swapped after reading before we interpret it as a host value.

#include <windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kLogCapacity = 128;
constexpr size_t kEntrySize = 32; // sizeof(LogEntry) on the target - keep in sync with debug_log.hpp
constexpr size_t kHeaderSize = 16; // magic(8) + writeIndex(4) + capacity(4)
constexpr size_t kBufferSize = kHeaderSize + kEntrySize * kLogCapacity;

// "WIIXG101" as raw bytes - matches kLogMagic in debug_log.hpp stored big-endian,
// which for an ASCII-safe constant means the bytes are just the string itself.
const uint8_t kMagicBytes[8] = { 'W', 'I', 'I', 'X', 'G', '1', '0', '1' };

uint32_t Bswap32(uint32_t v) {
    return _byteswap_ulong(v);
}

float BytesToFloatBE(const uint8_t* p) {
    uint32_t raw;
    memcpy(&raw, p, 4);
    raw = Bswap32(raw);
    float f;
    memcpy(&f, &raw, 4);
    return f;
}

uint32_t BytesToU32BE(const uint8_t* p) {
    uint32_t raw;
    memcpy(&raw, p, 4);
    return Bswap32(raw);
}

const char* TagName(uint32_t tag) {
    switch (tag) {
        case 1: return "PING";
        case 2: return "VPAD";
        case 3: return "KPAD";
        case 4: return "CAM_ENTRY";
        case 5: return "CAM_SKIP_ORIG";
        case 6: return "CAM_DEREF";
        case 7: return "CAM_INIT";
        case 8: return "CAM_PRE_WRITE";
        case 9: return "CAM_POST_ORIG";
        default: return "UNKNOWN";
    }
}

const char* VecName(uint32_t u0) {
    switch (u0) {
        case 0: return "pos";
        case 1: return "at";
        case 2: return "up";
        default: return "?";
    }
}

struct DecodedEntry {
    uint32_t tag;
    uint32_t frame;
    float f0, f1, f2, f3;
    uint32_t u0;
};

DecodedEntry DecodeEntry(const uint8_t* raw) {
    DecodedEntry e;
    e.tag   = BytesToU32BE(raw + 0);
    e.frame = BytesToU32BE(raw + 4);
    e.f0    = BytesToFloatBE(raw + 8);
    e.f1    = BytesToFloatBE(raw + 12);
    e.f2    = BytesToFloatBE(raw + 16);
    e.f3    = BytesToFloatBE(raw + 20);
    e.u0    = BytesToU32BE(raw + 24);
    return e;
}

void PrintEntry(const DecodedEntry& e) {
    switch (e.tag) {
        case 1: // PING
            printf("[frame %6u] PING       f=(%.3f,%.3f,%.3f,%.3f) u0=0x%08X\n",
                   e.frame, e.f0, e.f1, e.f2, e.f3, e.u0);
            break;
        case 2: // VPAD
            printf("[frame %6u] VPAD       hold=0x%04X  L=(%.3f,%.3f) R=(%.3f,%.3f)\n",
                   e.frame, e.u0, e.f0, e.f1, e.f2, e.f3);
            break;
        case 3: // KPAD
            printf("[frame %6u] KPAD       hold=0x%04X  L=(%.3f,%.3f) R=(%.3f,%.3f)\n",
                   e.frame, e.u0, e.f0, e.f1, e.f2, e.f3);
            break;
        case 4: // CAM_ENTRY
            printf("[frame %6u] CAM_ENTRY  obj=0x%08X\n", e.frame, e.u0);
            break;
        case 5: { // CAM_SKIP_ORIG
            const char* reason = e.u0 == 0 ? "freecam inactive"
                               : e.u0 == 1 ? "null camera"
                               : e.u0 == 2 ? "implausible pointer"
                               : e.u0 == 3 ? "not the locked-on camera"
                               : "geometry doesn't look real";
            printf("[frame %6u] CAM_SKIP   reason=%-24s lookDist=%.3f\n", e.frame, reason, e.f0);
            break;
        }
        case 6: // CAM_DEREF
            printf("[frame %6u] CAM_DEREF  %-3s = (%.4f, %.4f, %.4f)\n",
                   e.frame, VecName(e.u0), e.f0, e.f1, e.f2);
            break;
        case 7: // CAM_INIT
            printf("[frame %6u] CAM_INIT   hDist=%.4f yaw=%.4f pitch=%.4f\n",
                   e.frame, e.f0, e.f1, e.f2);
            break;
        case 8: // CAM_PRE_WRITE
            printf("[frame %6u] PRE_WRITE  %-3s = (%.4f, %.4f, %.4f)\n",
                   e.frame, VecName(e.u0), e.f0, e.f1, e.f2);
            break;
        case 9: // CAM_POST_ORIG
            printf("[frame %6u] POST_ORIG  %-3s = (%.4f, %.4f, %.4f)\n",
                   e.frame, VecName(e.u0), e.f0, e.f1, e.f2);
            break;
        default:
            printf("[frame %6u] tag=%u f=(%.3f,%.3f,%.3f,%.3f) u0=0x%08X\n",
                   e.frame, e.tag, e.f0, e.f1, e.f2, e.f3, e.u0);
            break;
    }
}

DWORD FindProcessId(const std::wstring& processName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    DWORD pid = 0;

    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

// Validates that `addr` really is the start of our ring buffer: magic must
// match AND the capacity field (right after writeIndex) must equal kLogCapacity.
// Guards against the 8-byte magic pattern coincidentally appearing elsewhere.
bool ValidateBufferAt(HANDLE proc, uint8_t* addr) {
    uint8_t header[kHeaderSize];
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(proc, addr, header, sizeof(header), &bytesRead) || bytesRead != sizeof(header)) {
        return false;
    }
    if (memcmp(header, kMagicBytes, sizeof(kMagicBytes)) != 0) return false;
    uint32_t capacity = BytesToU32BE(header + 12);
    return capacity == kLogCapacity;
}

// Scans all committed, readable regions of the target process for the magic
// cookie. Returns nullptr if not found.
uint8_t* ScanForRingBuffer(HANDLE proc) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    uint8_t* addr = static_cast<uint8_t*>(si.lpMinimumApplicationAddress);
    uint8_t* maxAddr = static_cast<uint8_t*>(si.lpMaximumApplicationAddress);

    std::vector<uint8_t> chunk;
    size_t regionsScanned = 0;

    while (addr < maxAddr) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(proc, addr, &mbi, sizeof(mbi)) != sizeof(mbi)) break;

        bool readable = (mbi.State == MEM_COMMIT) &&
                         !(mbi.Protect & PAGE_GUARD) &&
                         (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));

        if (readable && mbi.RegionSize > 0) {
            regionsScanned++;
            chunk.resize(mbi.RegionSize);
            SIZE_T bytesRead = 0;
            if (ReadProcessMemory(proc, mbi.BaseAddress, chunk.data(), mbi.RegionSize, &bytesRead) && bytesRead >= sizeof(kMagicBytes)) {
                // Naive substring scan for the 8-byte magic pattern.
                for (size_t i = 0; i + sizeof(kMagicBytes) <= bytesRead; ++i) {
                    if (memcmp(chunk.data() + i, kMagicBytes, sizeof(kMagicBytes)) == 0) {
                        uint8_t* candidate = static_cast<uint8_t*>(mbi.BaseAddress) + i;
                        if (ValidateBufferAt(proc, candidate)) {
                            printf("Found ring buffer at 0x%p (scanned %zu regions)\n", candidate, regionsScanned);
                            return candidate;
                        }
                    }
                }
            }
        }

        uint8_t* next = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
        if (next <= addr) break; // safety against zero-size regions looping forever
        addr = next;
    }

    printf("Ring buffer not found (scanned %zu regions).\n", regionsScanned);
    return nullptr;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring processName = L"Cemu.exe";
    if (argc > 1) processName = argv[1];

    printf("WiiXLaunch ring_log_reader - target process: %ls\n", processName.c_str());

    DWORD pid = FindProcessId(processName);
    if (pid == 0) {
        printf("Process '%ls' not found. Is Cemu running?\n", processName.c_str());
        return 1;
    }
    printf("Found process PID %lu\n", pid);

    HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!proc) {
        printf("OpenProcess failed (error %lu). Try running this tool as Administrator.\n", GetLastError());
        return 1;
    }

    uint8_t* bufferAddr = nullptr;
    uint32_t lastSeenIndex = 0;
    bool haveBaseline = false;

    for (;;) {
        if (!bufferAddr) {
            printf("Scanning process memory for ring buffer magic cookie...\n");
            bufferAddr = ScanForRingBuffer(proc);
            if (!bufferAddr) {
                printf("Retrying in 3 seconds...\n");
                Sleep(3000);
                continue;
            }
            haveBaseline = false;
        }

        uint8_t header[kHeaderSize];
        SIZE_T bytesRead = 0;
        bool ok = ReadProcessMemory(proc, bufferAddr, header, sizeof(header), &bytesRead) && bytesRead == sizeof(header);

        if (!ok || memcmp(header, kMagicBytes, sizeof(kMagicBytes)) != 0) {
            printf("Lost the ring buffer (process exited or buffer moved) - rescanning.\n");
            bufferAddr = nullptr;
            continue;
        }

        uint32_t writeIndex = BytesToU32BE(header + 8);

        if (!haveBaseline) {
            // On first successful attach, don't replay whatever history is
            // already in the buffer - start watching from "now".
            lastSeenIndex = writeIndex;
            haveBaseline = true;
            printf("Attached. Waiting for new entries (writeIndex=%u)...\n", writeIndex);
        }

        uint32_t pending = writeIndex - lastSeenIndex; // wraps correctly for uint32_t
        if (pending > kLogCapacity) {
            printf("[warning] overwrote %u entries before they were read - skipping ahead\n",
                   pending - kLogCapacity);
            lastSeenIndex = writeIndex - kLogCapacity;
            pending = kLogCapacity;
        }

        for (uint32_t i = 0; i < pending; ++i) {
            uint32_t idx = lastSeenIndex + i;
            uint32_t slot = idx % kLogCapacity;
            uint8_t* entryAddr = bufferAddr + kHeaderSize + slot * kEntrySize;

            uint8_t raw[kEntrySize];
            SIZE_T got = 0;
            if (ReadProcessMemory(proc, entryAddr, raw, sizeof(raw), &got) && got == sizeof(raw)) {
                PrintEntry(DecodeEntry(raw));
            }
        }
        lastSeenIndex = writeIndex;

        Sleep(16); // ~60Hz poll
    }
}
