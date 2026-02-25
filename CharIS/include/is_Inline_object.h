#pragma once
#include "is_base.h"
#include "is_blob_string.h"

namespace CharIS {

    struct ITsTextRenderer;
    struct TextRenderParam;

    /// <summary>
    /// 
    /// </summary>
    struct BaseCellBox {

        fp26dot6_t          advance;

        fp26dot6_t          ascent;

        fp26dot6_t          descent;

    };

    /// <summary>
    /// 
    /// </summary>
    struct IS_INTERFACE IISInlineObject : IISBase {

        virtual CODE AppendString(IISStringU16*) noexcept = 0;

        virtual void GetCellBox(BaseCellBox&) noexcept = 0;

    };


}