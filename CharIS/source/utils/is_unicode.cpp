#include "is_unicode.h"
#include <string>
namespace CharIS {
/// <summary>
/// simplified chinese-japanese-korean character loop-up-table
/// </summary>
static const uint32_t CJK_LUT[] = {
    0x00000000, 0xfffc0000, 0xffffffff, 0xffffffff,
    0xffffffff, 0x00000000, 0x00000000, 0x06000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0x07000fff,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
};
}

/// <summary>
/// Determines whether the specified ch is CJK.
/// </summary>
/// <param name="ch">The ch.</param>
/// <returns></returns>
uint32_t CharIS::IsCjk(char32_t ch) noexcept
{
    // BLOCK ALIGN AS 0x100
    const uint32_t ch2 = uint32_t(ch & 0x3ffff) >> 8;
    const uint32_t index = ch2 >> 5;
    const uint32_t mask = 1 << (ch2 & 0x1f);
    return CJK_LUT[index] & mask;
}

/// <summary>
/// Determines whether the specified ch is space.
/// </summary>
/// <param name="ch">The ch.</param>
/// <returns></returns>
uint32_t CharIS::IsSpace(char32_t ch) noexcept
{
    return ch == ' ' || ch == '\t';
}

/// <summary>
/// Determines whether [is space ex] [the specified ].
/// </summary>
/// <param name="ch">The ch.</param>
/// <returns></returns>
uint32_t CharIS::IsSpaceEx(char32_t ch) noexcept
{
    return ch == ' ' || ch == '\t' || ch == 0x3000;
}

/// <summary>
/// Lengthes the specified string.
/// </summary>
/// <param name="str">The string.</param>
/// <returns></returns>
uint32_t CharIS::Length(const char16_t * str) noexcept
{
    return static_cast<uint32_t>(std::char_traits<char16_t>::length(str));
}
