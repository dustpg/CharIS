#pragma once

#include "is_base.h"

namespace CharIS {

    struct BlobView { const uint8_t* ptr; size_t len; };
    struct U16View { const char16_t* ptr; size_t len; };

    /// <summary>
    /// binary large object
    /// </summary>
    struct IS_INTERFACE IISBlob : IISBase {

        virtual auto GetView() const noexcept -> BlobView = 0;

        virtual CODE Append(BlobView) noexcept = 0;

    };

    /// <summary>
    /// 
    /// </summary>
    struct IS_INTERFACE IISStringU16 : IISBase{

        virtual auto GetView() const noexcept -> U16View = 0;

        virtual CODE Append(U16View) noexcept = 0;

    };

};