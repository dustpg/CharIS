#pragma once

#include "is_text_layout.h"
#include "is_blob_string.h"
#include "is_text_renderer.h"

namespace CharIS {

    struct IISInlineObject;

    // selection mode
    enum SELECTION_MODE : uint32_t {
        // SET TO ARGUMENT
        SELECTION_MODE_SET = 0,
        // Ctrl + A
        SELECTION_MODE_ALL,
        // [Left]
        SELECTION_MODE_FRONT,
        // [Right]
        SELECTION_MODE_BACK,
        // Ctrl + [Left]
        SELECTION_MODE_WORD_FRONT,
        // Ctrl + [Right]
        SELECTION_MODE_WORD_BACK,
        // [Up]
        SELECTION_MODE_RAISE,
        // [Down]
        SELECTION_MODE_LOWER,
        // [Home]
        SELECTION_MODE_HOME,
        // [End]
        SELECTION_MODE_END,
        // Ctrl + [Home]
        SELECTION_MODE_FIRST,
        // Ctrl + [End]
        SELECTION_MODE_LAST,
        // Ctrl + [Up]
        SELECTION_MODE_RAISE_VIEW,
        // Ctrl + [Down]
        SELECTION_MODE_LOWER_VIEW,
        // Alt + [Up]
        SELECTION_MODE_RAISE_SHIFT,
        // Alt + [Up]
        SELECTION_MODE_LOWER_SHIFT,
    };


    /// <summary>
    /// 
    /// </summary>
    enum TEXT_LINE_TERMINATOR : uint32_t {
        // \n
        TEXT_LINE_TERMINATOR_LF = 0,
        // \r \n [DEFAULT ON WINDOWS]
        TEXT_LINE_TERMINATOR_CRLF,
        // \r
        TEXT_LINE_TERMINATOR_CR,
    };

    /// <summary>
    /// 
    /// </summary>
    enum TEXT_DOC_ATTR : uint32_t {
        // TYPE: TEXT_LINE_TERMINATOR
        TEXT_DOC_ATTR_LINE_TERMINATOR = 0,
        // TYPE: BOOL
        TEXT_DOC_ATTR_RICH_TEXT,
        // TYPE: CHAR32(0->NON PASSWORD MODE)
        TEXT_DOC_ATTR_PASSWORD_CHAR32,
        // TYPE: BOOL
        TEXT_DOC_ATTR_KERNING,
        // TYPE: BOOL
        TEXT_DOC_ATTR_SINGLE_LINE,
        // TYPE: COLOR(INT32)
        TEXT_DOC_ATTR_SELECTION_FOREGROUND,
        // TYPE: COLOR(INT32)
        TEXT_DOC_ATTR_SELECTION_BACKGROUND,
        // TYPE: 26.6 point
        TEXT_DOC_ATTR_LINE_HEIGHT_ASCENT,
        // TYPE: 26.6 point
        TEXT_DOC_ATTR_LINE_HEIGHT_DESCENT,
        // TYPE: WRAP_MODE
        TEXT_DOC_ATTR_WRAP_MODE,
        // TYPE: TEXT_ALIGN
        TEXT_DOC_ATTR_TEXT_ALIGN,
        // TYPE: VERTICAL_ALIGN
        TEXT_DOC_ATTR_VERTICAL_ALIGN,
        // TYPE: 26.6 point
        TEXT_DOC_ATTR_PARAGRAPH_HEIGHT_ASCENT,
        // TYPE: 26.6 point
        TEXT_DOC_ATTR_PARAGRAPH_HEIGHT_DESCENT,

    };

    /// <summary>
    /// 
    /// </summary>
    enum TEXT_DOC_POSITON : uint32_t {

        TEXT_DOC_POSITON_CARET = uint32_t(-1),

    };


    /// <summary>
    /// 
    /// </summary>
    struct TextSelection {

        uint32_t    caret;

        uint32_t    anchor;
    };


    /// <summary>
    /// 
    /// </summary>
    struct IS_INTERFACE IISTextDocument : IISTextLayout {

        // ---------------- DOC

        virtual CODE Serialize(IISBlob* blob, const char* format = "") noexcept = 0;

        virtual CODE Deserialize(IISBlob* blob) noexcept = 0;

        virtual CODE SetSelection(SELECTION_MODE mode, uint32_t pos, bool keepAnchor) noexcept = 0;

        virtual CODE GetSelection(TextSelection sel[], uint32_t& count) noexcept = 0;

        virtual CODE SetAttribute(TEXT_DOC_ATTR attr, int32_t value) noexcept = 0;

        virtual CODE InsertText(uint32_t position, const char16_t* text, uint32_t len = uint32_t(-1), uint32_t* inserted=nullptr) noexcept = 0;

        virtual CODE RemoveText(Range range, uint32_t* removed = nullptr) noexcept = 0;

        virtual CODE MakePlainString(Range, IISStringU16* string) noexcept = 0;

        virtual CODE SetDefualtEffect(const TextEffect&) noexcept = 0;

        virtual CODE SetInlineObject(Range, IISInlineObject*) noexcept = 0;

        // ---------------- UNDO

        virtual CODE BeginOp() noexcept = 0;

        virtual CODE EndOp() noexcept = 0;

        virtual CODE Undo() noexcept = 0;

        virtual CODE Redo() noexcept = 0;

        virtual CODE SetUndoLimit(uint32_t limit) noexcept = 0;

        virtual CODE ClearUndo() noexcept = 0;

    };

}


extern "C"
CharIS::CODE CharisCreateTextDocument(
    CharIS::IISTextDocument** layout,
    CharIS::IISTextRenderer* renderer,
    const char16_t* defaultFontName = u"",
    CharIS::fp26dot6_t defaultFontSize = 12 << 6,
    int flag = 0
) noexcept;
