#pragma once

#include <CharIS/include/is_base.h>


namespace CharIS {

    CODE FtError(int) noexcept;

    void* FtOpenFile(const char16_t*) noexcept;

    uint32_t FtFileSize(void*) noexcept;

    uint32_t FtFileRead(void*, uint32_t offset, uint8_t buffer[], uint32_t buflen) noexcept;

    void FtFileClose(void*) noexcept;

}