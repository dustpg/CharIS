#pragma once
#include "is_base.h"

namespace CharIS {
#if 1
    struct PointF { double x, y; };

    struct IS_INTERFACE IISGeometrySink {

        virtual void MoveTo(PointF to) noexcept = 0;

        virtual void LineTo(PointF to) noexcept = 0;

        virtual void ConicTo(PointF ctrl, PointF to) noexcept = 0;

        virtual void CubicTo(PointF ctrl1, PointF ctrl2, PointF to) noexcept = 0;

    };
#else

    struct Point { int32_t x, y; };

    struct IS_INTERFACE IISGeometrySink {

        virtual void MoveTo(Point to) noexcept = 0;

        virtual void LineTo(Point to) noexcept = 0;

        virtual void ConicTo(Point ctrl, Point to) noexcept = 0;

        virtual void CubicTo(Point ctrl1, Point ctrl2, Point to) noexcept = 0;

    };

#endif
}
