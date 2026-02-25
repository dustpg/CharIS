#include "../core/is_ft2_helper.h"
#include <Windows.h>

/// <summary>
/// Fts the open file.
/// </summary>
/// <param name="">The .</param>
/// <returns></returns>
void* CharIS::FtOpenFile(const char16_t* file) noexcept
{
    return ::CreateFileW(
        (const wchar_t*)file,
        GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr
    );
}


/// <summary>
/// Fts the size of the file.
/// </summary>
/// <param name="">The .</param>
/// <returns></returns>
uint32_t CharIS::FtFileSize(void* fileHandle) noexcept
{
    return ::GetFileSize((HANDLE)fileHandle, nullptr);
}


/// <summary>
/// Fts the file read.
/// </summary>
/// <param name="fileHandle">The file handle.</param>
/// <param name="offset">The offset.</param>
/// <param name="buffer">The buffer.</param>
/// <param name="count">The count.</param>
/// <returns></returns>
uint32_t CharIS::FtFileRead(void* fileHandle, uint32_t offset, unsigned char* buffer, uint32_t count) noexcept
{
    ::SetFilePointer((HANDLE)fileHandle, offset, nullptr, FILE_BEGIN);
    DWORD bytesRead;
    if (!::ReadFile((HANDLE)fileHandle, buffer, count, &bytesRead, nullptr))
        return 0;
    return bytesRead;
}


/// <summary>
/// Fts the file close.
/// </summary>
/// <param name="fileHandle">The file handle.</param>
/// <returns></returns>
void CharIS::FtFileClose(void* fileHandle) noexcept
{
    ::CloseHandle((HANDLE)fileHandle);
}
