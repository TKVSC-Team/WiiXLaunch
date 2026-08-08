#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#else
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint32_t kBufferCapacity = 4096; // bytes (must match kBufferCapacity in debug_log.hpp)
constexpr size_t kHeaderSize = 16; // magic(8) + writeIndex(4) + capacity(4)
constexpr size_t kScanChunkSize = 4 * 1024 * 1024; // read regions in bounded chunks, not all at once

const uint8_t kMagicBytes[8] = { 'W', 'I', 'I', 'X', 'G', '1', '0', '1' };
const uint8_t kEntryStart[4] = { 'W', 'X', '[', '[' };
const uint8_t kEntryEnd[4]   = { 'W', 'X', ']', ']' };

uint32_t Bswap32(uint32_t v) {
#ifdef _WIN32
    return _byteswap_ulong(v);
#else
    return __builtin_bswap32(v);
#endif
}

uint32_t BytesToU32BE(const uint8_t* p) {
    uint32_t raw;
    memcpy(&raw, p, 4);
    return Bswap32(raw);
}

bool EqualsIgnoreCase(const char* a, const char* b) {
#ifdef _WIN32
    return _stricmp(a, b) == 0;
#else
    return strcasecmp(a, b) == 0;
#endif
}

#ifdef _WIN32

struct ProcHandle {
    HANDLE h = nullptr;
    bool Valid() const { return h != nullptr; }
};

uint32_t FindProcessId(const std::string& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(entry);
    uint32_t pid = 0;

    if (Process32First(snap, &entry)) {
        do {
            if (EqualsIgnoreCase(entry.szExeFile, name.c_str())) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32Next(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

ProcHandle OpenTarget(uint32_t pid) {
    ProcHandle proc;
    proc.h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    return proc;
}

void PrintOpenTargetError(uint32_t /*pid*/) {
    printf("OpenProcess failed (error %lu). Try running this tool as Administrator.\n", GetLastError());
}

bool ReadMem(ProcHandle proc, uintptr_t addr, void* buf, size_t len) {
    SIZE_T got = 0;
    return ReadProcessMemory(proc.h, reinterpret_cast<LPCVOID>(addr), buf, len, &got) && got == len;
}

template <typename Fn>
bool ForEachReadableRegion(ProcHandle proc, Fn&& fn) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    uint8_t* addr = static_cast<uint8_t*>(si.lpMinimumApplicationAddress);
    uint8_t* maxAddr = static_cast<uint8_t*>(si.lpMaximumApplicationAddress);

    while (addr < maxAddr) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(proc.h, addr, &mbi, sizeof(mbi)) != sizeof(mbi)) break;

        bool readable = (mbi.State == MEM_COMMIT) &&
                         !(mbi.Protect & PAGE_GUARD) &&
                         (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));

        if (readable && mbi.RegionSize > 0) {
            if (fn(reinterpret_cast<uintptr_t>(mbi.BaseAddress), static_cast<size_t>(mbi.RegionSize))) return true;
        }

        uint8_t* next = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
        if (next <= addr) break; // safety against zero-size regions looping forever
        addr = next;
    }
    return false;
}

#else // Linux

struct ProcHandle {
    int fd = -1;
    uint32_t pid = 0;
    bool Valid() const { return fd >= 0; }
};

uint32_t FindProcessId(const std::string& name) {
    DIR* dir = opendir("/proc");
    if (!dir) return 0;

    uint32_t pid = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (!isdigit(static_cast<unsigned char>(entry->d_name[0]))) continue;

        char path[64];
        snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
        FILE* f = fopen(path, "r");
        if (!f) continue;

        char comm[256] = {};
        bool matched = false;
        if (fgets(comm, sizeof(comm), f)) {
            size_t n = strlen(comm);
            if (n && comm[n - 1] == '\n') comm[n - 1] = '\0';
            matched = EqualsIgnoreCase(comm, name.c_str());
        }
        fclose(f);

        if (matched) {
            pid = static_cast<uint32_t>(atoi(entry->d_name));
            break;
        }
    }
    closedir(dir);
    return pid;
}

ProcHandle OpenTarget(uint32_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%u/mem", pid);
    ProcHandle proc;
    proc.fd = open(path, O_RDONLY);
    proc.pid = pid;
    return proc;
}

void PrintOpenTargetError(uint32_t pid) {
    printf("Failed to open /proc/%u/mem (%s). Try running this tool with sudo.\n", pid, strerror(errno));
}

bool ReadMem(ProcHandle proc, uintptr_t addr, void* buf, size_t len) {
    ssize_t got = pread(proc.fd, buf, len, static_cast<off_t>(addr));
    return got == static_cast<ssize_t>(len);
}

// Calls fn(base, size) for each readable region listed in /proc/<pid>/maps.
// Stops early (and returns true) if fn returns true.
template <typename Fn>
bool ForEachReadableRegion(ProcHandle proc, Fn&& fn) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%u/maps", proc.pid);
    FILE* f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    bool stopped = false;
    while (!stopped && fgets(line, sizeof(line), f)) {
        unsigned long long start = 0, end = 0;
        char perms[8] = {};
        if (sscanf(line, "%llx-%llx %7s", &start, &end, perms) != 3) continue;
        if (perms[0] != 'r' || end <= start) continue; // skip non-readable/zero-size regions

        stopped = fn(static_cast<uintptr_t>(start), static_cast<size_t>(end - start));
    }
    fclose(f);
    return stopped;
}

#endif

bool ValidateBufferAt(ProcHandle proc, uintptr_t addr) {
    uint8_t header[kHeaderSize];
    if (!ReadMem(proc, addr, header, sizeof(header))) return false;
    if (memcmp(header, kMagicBytes, sizeof(kMagicBytes)) != 0) return false;
    return BytesToU32BE(header + 12) == kBufferCapacity;
}

uintptr_t ScanRegionForBuffer(ProcHandle proc, uintptr_t base, size_t size) {
    std::vector<uint8_t> chunk(kScanChunkSize);
    size_t overlap = sizeof(kMagicBytes) - 1;
    size_t offset = 0;

    while (offset < size) {
        size_t want = std::min(kScanChunkSize, size - offset);
        if (!ReadMem(proc, base + offset, chunk.data(), want)) break;

        for (size_t i = 0; i + sizeof(kMagicBytes) <= want; i++) {
            if (memcmp(chunk.data() + i, kMagicBytes, sizeof(kMagicBytes)) == 0) {
                uintptr_t candidate = base + offset + i;
                if (ValidateBufferAt(proc, candidate)) return candidate;
            }
        }

        if (want <= overlap) break;
        offset += want - overlap;
    }
    return 0;
}

uintptr_t ScanForRingBuffer(ProcHandle proc) {
    size_t regionsScanned = 0;
    uintptr_t found = 0;

    ForEachReadableRegion(proc, [&](uintptr_t base, size_t size) {
        regionsScanned++;
        uintptr_t candidate = ScanRegionForBuffer(proc, base, size);
        if (candidate) {
            found = candidate;
            return true;
        }
        return false;
    });

    if (found) {
        printf("Found ring buffer at 0x%llx (scanned %zu regions)\n", static_cast<unsigned long long>(found), regionsScanned);
    } else {
        printf("Ring buffer not found (scanned %zu regions).\n", regionsScanned);
    }
    return found;
}

bool ReadRingBytes(ProcHandle proc, uintptr_t bufferAddr, uint32_t start, uint32_t count, std::vector<uint8_t>& out) {
    out.resize(count);
    uint32_t offset = start % kBufferCapacity;
    uint32_t firstLen = std::min<uint32_t>(count, kBufferCapacity - offset);
    if (!ReadMem(proc, bufferAddr + kHeaderSize + offset, out.data(), firstLen)) return false;

    if (firstLen < count) {
        uint32_t remaining = count - firstLen;
        if (!ReadMem(proc, bufferAddr + kHeaderSize, out.data() + firstLen, remaining)) return false;
    }
    return true;
}

void PrintEntries(const std::vector<uint8_t>& buf) {
    size_t i = 0;
    while (i + 4 <= buf.size()) {
        if (memcmp(&buf[i], kEntryStart, 4) != 0) { i++; continue; }

        size_t textStart = i + 4;
        size_t j = textStart;
        bool found = false;
        while (j + 4 <= buf.size()) {
            if (memcmp(&buf[j], kEntryEnd, 4) == 0) { found = true; break; }
            j++;
        }
        if (!found) break;

        std::string text(reinterpret_cast<const char*>(&buf[textStart]), j - textStart);
        printf("[log] %s\n", text.c_str());
        i = j + 4;
    }
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    std::string processName = "Cemu.exe";
#else
    std::string processName = "Cemu";
#endif
    if (argc > 1) processName = argv[1];

    printf("WiiXLaunch ring_log_reader - target process: %s\n", processName.c_str());

    uint32_t pid = FindProcessId(processName);
    if (pid == 0) {
        printf("Process '%s' not found. Is Cemu running?\n", processName.c_str());
        return 1;
    }
    printf("Found process PID %u\n", pid);

    ProcHandle proc = OpenTarget(pid);
    if (!proc.Valid()) {
        PrintOpenTargetError(pid);
        return 1;
    }

    uintptr_t bufferAddr = 0;
    uint32_t lastSeenIndex = 0;
    bool haveBaseline = false;

    for (;;) {
        if (!bufferAddr) {
            printf("Scanning process memory for ring buffer magic cookie...\n");
            bufferAddr = ScanForRingBuffer(proc);
            if (!bufferAddr) {
                printf("Retrying in 3 seconds...\n");
                std::this_thread::sleep_for(std::chrono::seconds(3));
                continue;
            }
            haveBaseline = false;
        }

        uint8_t header[kHeaderSize];
        bool ok = ReadMem(proc, bufferAddr, header, sizeof(header));

        if (!ok || memcmp(header, kMagicBytes, sizeof(kMagicBytes)) != 0) {
            printf("Lost the ring buffer (process exited or buffer moved) - rescanning.\n");
            bufferAddr = 0;
            continue;
        }

        uint32_t writeIndex = BytesToU32BE(header + 8);

        if (!haveBaseline) {
            // On first successful attach, don't replay whatever history is already in the buffer.
            lastSeenIndex = writeIndex;
            haveBaseline = true;
            printf("Attached. Waiting for new entries (writeIndex=%u)...\n", writeIndex);
        }

        uint32_t pending = writeIndex - lastSeenIndex;
        if (pending > kBufferCapacity) {
            printf("[warning] overwrote %u bytes before they were read - skipping ahead\n",
                   pending - kBufferCapacity);
            lastSeenIndex = writeIndex - kBufferCapacity;
            pending = kBufferCapacity;
        }

        if (pending > 0) {
            std::vector<uint8_t> raw;
            if (ReadRingBytes(proc, bufferAddr, lastSeenIndex, pending, raw)) {
                PrintEntries(raw);
            }
        }
        lastSeenIndex = writeIndex;

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60Hz poll
    }
}
