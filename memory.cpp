#include "memory.hpp"
#include <stdexcept>

auto mm::internal::build_read_syscall() -> nt_read_fn
    {
    return [ ] ( ) -> nt_read_fn
        {
        static constexpr std::array<uint8_t, 11> stub = {
            0x4C, 0x8B, 0xD1,
            0xB8, 0x3F, 0x00, 0x00, 0x00, 
            0x0F, 0x05,
            0xC3
            };

        void *exec = VirtualAlloc ( nullptr, stub.size ( ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
        if ( !exec )
             throw std::runtime_error ( std::format ( "[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__ ) );

        std::memcpy ( exec, stub.data ( ), stub.size ( ) );
        return reinterpret_cast< nt_read_fn >( exec );
        }( );
    }

auto mm::internal::build_write_syscall ( ) -> nt_write_fn
    {
    return [ ] ( ) -> nt_write_fn
        {
        static constexpr std::array<uint8_t, 11> stub = {
            0x4C, 0x8B, 0xD1,
            0xB8, 0x3A, 0x00, 0x00, 0x00,
            0x0F, 0x05,
            0xC3
            };

        void *exec = VirtualAlloc ( nullptr, stub.size ( ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
        if ( !exec )
            throw std::runtime_error ( std::format ( "[mm] VirtualAlloc failure at {}:{}", __FILE__, __LINE__ ) );

        std::memcpy ( exec, stub.data ( ), stub.size ( ) );
        return reinterpret_cast< nt_write_fn >( exec );
        }( );
    }

auto mm::internal::find_process_id ( std::string_view process_name ) -> std::uintptr_t
    {
    return [&process_name ] ( ) -> std::uintptr_t
        {
        PROCESSENTRY32 entry { .dwSize = sizeof ( PROCESSENTRY32 ) };
        const auto snap = std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype( &CloseHandle )> {
            CreateToolhelp32Snapshot ( TH32CS_SNAPPROCESS, 0 ), CloseHandle
            };
        if ( !Process32First ( snap.get ( ), &entry ) )  
            return 0;
        while ( Process32Next ( snap.get ( ), &entry ) )
            {
            if ( process_name == entry.szExeFile )
                return entry.th32ProcessID;
            }
        return 0;
        }( );
    }

mm::internal::memory_manager::memory_manager ( )
{
    m_read = build_read_syscall ( );
    m_write = build_write_syscall ( );
}

mm::internal::memory_manager::~memory_manager()
{
    this->close ( );
}

auto mm::internal::memory_manager::open ( std::string_view process_name ) -> bool
    {
    return [this, &process_name ] ( ) -> bool
        {
        const std::uintptr_t pid = find_process_id ( process_name );
        if ( !pid ) return false;
        this->proc_handle = OpenProcess ( PROCESS_ALL_ACCESS, FALSE, static_cast< DWORD >( pid ) );
        return this->proc_handle != nullptr;
        }( );
    }

auto mm::internal::memory_manager::close ( ) -> void
    {
    return [this ] ( ) -> void
        {
        if ( !this->proc_handle ) return;
        CloseHandle ( this->proc_handle );
        this->proc_handle = nullptr;
        }( );
    }

auto mm::internal::memory_manager::get_module_base ( std::string_view module_name ) const -> std::uintptr_t
    {
    return [this, &module_name ] ( ) -> std::uintptr_t
        {
        if ( !this->proc_handle )
            return 0;

        std::array<HMODULE, 1024> modules {};
        DWORD bytes_needed = 0;
        if ( !EnumProcessModules ( this->proc_handle, modules.data ( ), sizeof ( modules ), &bytes_needed ) )
            return 0;

        const auto count = bytes_needed / sizeof ( HMODULE );
        const auto it = std::find_if ( modules.begin ( ), modules.begin ( ) + count, [ & ] ( HMODULE mod )
                                       {
                                       std::array<char, MAX_PATH> buf {};
                                       return GetModuleBaseNameA ( this->proc_handle, mod, buf.data ( ), buf.size ( ) ) && module_name == buf.data ( );
                                       } );
        return it != modules.begin ( ) + count ? reinterpret_cast< std::uintptr_t >( *it ) : 0;
        }( ); // vs i love u so much, that auto formatting is just beautiful :heart:
    }

auto mm::internal::memory_manager::read_raw_string ( std::uintptr_t address ) const -> std::string
    {
    return [this, &address ] ( ) -> std::string
        {
        std::string result {};
        result.reserve ( 128 );
        for ( int i = 0; i < 200; ++i )
            {
            const char c = this->read<char> ( address + i );
            if ( c == '\0' ) break;
            result += c;
            }
        return result;
        }( );
    }

auto mm::internal::memory_manager::read_string ( std::uintptr_t address ) const -> std::string
    {
    return [ this, &address] ( ) -> std::string {
        const bool is_heap = this->read<std::uintptr_t> ( address + 0x18 ) >= 16u;
        return this->read_raw_string ( is_heap ? this->read<std::uintptr_t> ( address ) : address );
        }( );
    }

//helper fn

auto mm::internal::memory_manager::get_handle ( ) -> HANDLE
    {
    return [this ] ( ) -> HANDLE
        {
        return this->proc_handle;
        }( );
    }

std::shared_ptr<mm::internal::memory_manager> mm::g_mm = std::make_shared<mm::internal::memory_manager>();
