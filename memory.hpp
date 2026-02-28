#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <string>
#include <cstdint>
#include <array>
#include <memory>
#include <format>

using nt_read_fn  = NTSTATUS(WINAPI*)(HANDLE, PVOID, PVOID, ULONG, PULONG);
using nt_write_fn = NTSTATUS(WINAPI*)(HANDLE, PVOID, PVOID, ULONG, PULONG);

namespace mm
	{

	namespace internal
		{

        nt_read_fn  build_read_syscall ( );
        nt_write_fn build_write_syscall ( );

        uintptr_t find_process_id ( std::string_view process_name );

        class memory_manager
            {
            public:
                HANDLE proc_handle = nullptr;

                memory_manager ( );
                ~memory_manager ( );

                //helper fn
                auto get_handle ( ) -> HANDLE;

                auto open ( std::string_view process_name ) -> bool;
                auto close ( ) -> void;

                auto get_module_base ( std::string_view module_name ) const -> std::uintptr_t;

                template<typename T>
                auto read ( std::uintptr_t address ) const -> T
                    {
                    return [ & ] ( ) -> T
                        {
                        T val {};
                        this->m_read ( this->proc_handle, reinterpret_cast< PVOID >( address ), &val, sizeof ( T ), nullptr );
                        return val;
                        }( );
                    }

                template<typename T>
                auto write ( std::uintptr_t address, T value ) const -> void
                    {
                    return [ & ] ( )->T
                        {
                        this->m_write ( this->proc_handle, reinterpret_cast< PVOID >( address ), &value, sizeof ( T ), nullptr );
                        }( );
                    
                    }

                auto read_string ( std::uintptr_t address ) const -> std::string;

            private:
                nt_read_fn  m_read = nullptr;
                nt_write_fn m_write = nullptr;

                auto read_raw_string ( std::uintptr_t address ) const -> std::string;
            };

		}

    extern std::shared_ptr<internal::memory_manager> g_mm;

	}
