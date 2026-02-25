#pragma once
#include <cstdint>

namespace CharIS {

    enum : uint32_t {
        FONT_FACE_NAME_LENGTH = 64,
    };


    /// <summary>
    /// The font weight enumeration describes common values for degree of blackness or thickness of strokes of characters in a font.
    /// Font weight values less than 1 or greater than 999 are considered to be invalid, and they are rejected by font API functions.
    /// </summary>
    enum FONT_WEIGHT : uint32_t {

        /// <summary>
        /// Predefined font weight : Thin (100).
        /// </summary>
        FONT_WEIGHT_THIN = 100,

        /// <summary>
        /// Predefined font weight : Extra-light (200).
        /// </summary>
        FONT_WEIGHT_EXTRA_LIGHT = 200,

        /// <summary>
        /// Predefined font weight : Ultra-light (200).
        /// </summary>
        FONT_WEIGHT_ULTRA_LIGHT = 200,

        /// <summary>
        /// Predefined font weight : Light (300).
        /// </summary>
        FONT_WEIGHT_LIGHT = 300,

        /// <summary>
        /// Predefined font weight : Semi-light (350).
        /// </summary>
        FONT_WEIGHT_SEMI_LIGHT = 350,

        /// <summary>
        /// Predefined font weight : Normal (400).
        /// </summary>
        FONT_WEIGHT_NORMAL = 400,

        /// <summary>
        /// Predefined font weight : Regular (400).
        /// </summary>
        FONT_WEIGHT_REGULAR = 400,

        /// <summary>
        /// Predefined font weight : Medium (500).
        /// </summary>
        FONT_WEIGHT_MEDIUM = 500,

        /// <summary>
        /// Predefined font weight : Demi-bold (600).
        /// </summary>
        FONT_WEIGHT_DEMI_BOLD = 600,

        /// <summary>
        /// Predefined font weight : Semi-bold (600).
        /// </summary>
        FONT_WEIGHT_SEMI_BOLD = 600,

        /// <summary>
        /// Predefined font weight : Bold (700).
        /// </summary>
        FONT_WEIGHT_BOLD = 700,

        /// <summary>
        /// Predefined font weight : Extra-bold (800).
        /// </summary>
        FONT_WEIGHT_EXTRA_BOLD = 800,

        /// <summary>
        /// Predefined font weight : Ultra-bold (800).
        /// </summary>
        FONT_WEIGHT_ULTRA_BOLD = 800,

        /// <summary>
        /// Predefined font weight : Black (900).
        /// </summary>
        FONT_WEIGHT_BLACK = 900,

        /// <summary>
        /// Predefined font weight : Heavy (900).
        /// </summary>
        FONT_WEIGHT_HEAVY = 900,

        /// <summary>
        /// Predefined font weight : Extra-black (950).
        /// </summary>
        FONT_WEIGHT_EXTRA_BLACK = 950,

        /// <summary>
        /// Predefined font weight : Ultra-black (950).
        /// </summary>
        FONT_WEIGHT_ULTRA_BLACK = 950
    };

    /// <summary>
    /// The font stretch enumeration describes relative change from the normal aspect ratio
    /// as specified by a font designer for the glyphs in a font.
    /// Values less than 1 or greater than 9 are considered to be invalid, and they are rejected by font API functions.
    /// </summary>
    enum FONT_STRETCH : uint32_t {

        /// <summary>
        /// Predefined font stretch : Not known (0).
        /// </summary>
        FONT_STRETCH_UNDEFINED = 0,

        /// <summary>
        /// Predefined font stretch : Ultra-condensed (1).
        /// </summary>
        FONT_STRETCH_ULTRA_CONDENSED = 1,

        /// <summary>
        /// Predefined font stretch : Extra-condensed (2).
        /// </summary>
        FONT_STRETCH_EXTRA_CONDENSED = 2,

        /// <summary>
        /// Predefined font stretch : Condensed (3).
        /// </summary>
        FONT_STRETCH_CONDENSED = 3,

        /// <summary>
        /// Predefined font stretch : Semi-condensed (4).
        /// </summary>
        FONT_STRETCH_SEMI_CONDENSED = 4,

        /// <summary>
        /// Predefined font stretch : Normal (5).
        /// </summary>
        FONT_STRETCH_NORMAL = 5,

        /// <summary>
        /// Predefined font stretch : Medium (5).
        /// </summary>
        FONT_STRETCH_MEDIUM = 5,

        /// <summary>
        /// Predefined font stretch : Semi-expanded (6).
        /// </summary>
        FONT_STRETCH_SEMI_EXPANDED = 6,

        /// <summary>
        /// Predefined font stretch : Expanded (7).
        /// </summary>
        FONT_STRETCH_EXPANDED = 7,

        /// <summary>
        /// Predefined font stretch : Extra-expanded (8).
        /// </summary>
        FONT_STRETCH_EXTRA_EXPANDED = 8,

        /// <summary>
        /// Predefined font stretch : Ultra-expanded (9).
        /// </summary>
        FONT_STRETCH_ULTRA_EXPANDED = 9
    };

    /// <summary>
    /// The font style enumeration describes the slope style of a font face, such as Normal, Italic or Oblique.
    /// Values other than the ones defined in the enumeration are considered to be invalid, and they are rejected by font API functions.
    /// </summary>
    enum FONT_STYLE : uint32_t {

        /// <summary>
        /// Font slope style : Normal.
        /// </summary>
        FONT_STYLE_NORMAL,

        /// <summary>
        /// Font slope style : Italic.
        /// </summary>
        FONT_STYLE_ITALIC

    };


    /// <summary>
    /// 
    /// </summary>
    struct Font {

        uint16_t        weight;

        uint8_t         stretch;

        uint8_t         style;
        // length of family
        uint16_t        length;

        char16_t        family[FONT_FACE_NAME_LENGTH+1];

    };


    /// <summary>
    /// 
    /// </summary>
    struct FontFamily {
        // length of family
        uint16_t        length;

        char16_t        family[FONT_FACE_NAME_LENGTH+1];

    };

    /// <summary>
    /// 
    /// </summary>
    enum TEXT_EFFECT_FLAG : uint32_t {
        // [FOR BACKGROUND]
        TEXT_EFFECT_FLAG_UNDERLINE           = 1 << 0,
        // [FOR BACKGROUND]
        TEXT_EFFECT_FLAG_STRIKETHROUGH       = 1 << 1,



        TEXT_EFFECT_FLAG_USER_CUSTOM0        = 1 << 12,
        TEXT_EFFECT_FLAG_USER_CUSTOM1        = 1 << 13,
        TEXT_EFFECT_FLAG_USER_CUSTOM2        = 1 << 14,
        TEXT_EFFECT_FLAG_USER_CUSTOM4        = 1 << 15,
    };


    /// <summary>
    /// custom effect for each char
    /// </summary>
    struct TextEffect {
        // COLOR[FOR BACKGROUND]
        uint32_t        background;
        // COLOR
        uint32_t        foreground;
        // COLOR
        uint32_t        outline;
        // CUSTOM
        uint32_t        flags;
    };

}