#include <cstdint>


namespace CharIS {
    // UTF-16: HIGH/FRONT
    inline bool IsHighSurrogate(char16_t ch) noexcept { return ((ch) & 0xFC00) == 0xD800; }
    // UTF-16: LOW/BACK
    inline bool IsLowSurrogate(char16_t ch) noexcept { return ((ch) & 0xFC00) == 0xDC00; }
    // PACK
    inline char32_t UCS4FromChar16x2(char16_t lead, char16_t trail) noexcept {
        // is_high_surrogate(lead) is_low_surrogate(trail) 
        return (char32_t)((lead - 0xD800) << 10 | (trail - 0xDC00)) + (0x10000);
    }
    // UNPACK
    inline uint32_t UCS4ToChar16x2(char32_t ucs4, char16_t buffer[]) noexcept {
        if (ucs4 <= 0xFFFF) {
            buffer[0] = static_cast<char16_t>(ucs4);
            return 1;
        }
        buffer[0] = static_cast<char16_t>(0xD800 + (ucs4 >> 10) - (0x10000 >> 10));
        buffer[1] = static_cast<char16_t>(0xDC00 + (ucs4 & 0x3FF));
        return 2;
    }
    // Is CJK
    uint32_t IsCjk(char32_t) noexcept;
    // Is SPACE(include \t)
    uint32_t IsSpace(char32_t) noexcept;
    // Is SPACE(include full-width char)
    uint32_t IsSpaceEx(char32_t) noexcept;
    // length
    uint32_t Length(const char16_t*) noexcept;
}