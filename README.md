# memory_manager

A small, self-contained Windows memory manager written in C++. It reads and writes memory in an external process using raw syscall stubs instead of going through ntdll, which means it works cleanly even when ntdll is hooked.

No external dependencies, no bloated wrapper classes, just a header and a source file you drop into your project.

---

## What's inside

| File | What it does |
|---|---|
| `memory_manager.hpp` | Class declaration + templated read/write |
| `memory_manager.cpp` | Syscall stubs, process helpers, string reading |

---

## How to use it

### 1. Add the files to your project

Just copy `memory_manager.hpp` and `memory_manager.cpp` into your project and include the header wherever you need it.

### 2. Open a process

```cpp
#include "memory_manager.hpp"

memory_manager mem;

if (!mem.open("target.exe"))
{
    // process not found or couldn't get a handle
    return 1;
}
```

### 3. Read memory

```cpp
// read any type you want
float health = mem.read<float>(0x12345678);
int ammo     = mem.read<int>(base + 0x50);
```

### 4. Write memory

```cpp
mem.write<float>(0x12345678, 100.0f);
mem.write<int>(base + 0x50, 999);
```

### 5. Get a module base address

```cpp
uintptr_t base = mem.get_module_base("target.exe");
```

### 6. Read a string

```cpp
// handles both small string optimisation (SSO) and heap-allocated strings
std::string name = mem.read_string(some_address);
```

---

## How the syscall stubs work

Instead of calling `ReadProcessMemory` / `WriteProcessMemory` (which go through ntdll and can be intercepted), this manager builds tiny assembly stubs at runtime and calls the NT kernel directly.

```
mov r10, rcx     ; required setup for syscalls on x64
mov eax, 0x3F   ; syscall number for NtReadVirtualMemory
syscall
ret
```

The stubs are allocated as executable memory with `VirtualAlloc` and cast to function pointers. That's it.

> **Note:** The syscall numbers (`0x3F` for read, `0x3A` for write) are valid for **Windows 10 / Windows 11**. If you're running a different build you might need to look up the correct numbers for your version.

---

## Requirements

- Windows 10 or 11 (x64)
- C++ 17
- Link against `Psapi.lib` (add it to your linker inputs or use `#pragma comment(lib, "Psapi.lib")`)

---

## Building

There's nothing special here. Add both files to your project and make sure you're compiling for **x64**. The syscall stubs are x64-only and will not work on 32-bit.

If you're using CMake a basic setup looks like this:

```cmake
cmake_minimum_required(VERSION 3.15)
project(my_project)

add_executable(my_project main.cpp memory_manager.cpp)
target_link_libraries(my_project Psapi)
```

---

## Notes

- The process handle is cleaned up automatically when `memory_manager` goes out of scope (RAII via the destructor).
- `read_string` handles the SSO (small string optimisation) layout used by MSVC's `std::string` — strings under 16 characters are stored inline, longer ones store a pointer to heap memory at offset `0x0`.
- `find_process_id` is exposed as a free function if you need the PID separately.

---

## License

Do whatever you want with it.
