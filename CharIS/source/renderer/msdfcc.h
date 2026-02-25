#ifndef MSDFCC_HEADER_H
#define MSDFCC_HEADER_H
/*
MIT License

Copyright (c) 2026        dustpg
Copyright (c) 2014 - 2025 Viktor Chlumsky

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

//#define msdfcc_malloc
//#define msdfcc_realloc
//#define msdfcc_free
//#define MSDFCC_USE_OPENMP
//#define MSDFCC_USE_FLOAT

#ifdef __cplusplus
extern "C" {
#else
#ifndef noexcept
#define noexcept
#endif
#endif

#ifdef MSDFCC_USE_FLOAT
typedef float msdfcc_float_t;
#else
typedef double msdfcc_float_t;
#endif

#define MSDFCC_LIT(V)    ((msdfcc_float_t)(V))

typedef struct {
    msdfcc_float_t x, y;
} msdfcc_point_t;

typedef struct {
    float r, g, b, a;
} msdfcc_rgba_t;

typedef struct {
    int32_t width;
    int32_t height;
} msdfcc_size2d_t;

static inline int32_t msdfcc_size2d_row_stride(msdfcc_size2d_t s) noexcept {
    return s.width;
}

static inline int msdfcc_points_equal(msdfcc_point_t a, msdfcc_point_t b) noexcept {
    return a.x == b.x && a.y == b.y;
}

static inline float msdfcc_clamp(float x) noexcept {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline uint8_t msdfcc_float2byte(float x) noexcept {
    float clamped = msdfcc_clamp(x);
    float scaled = 255.0f * clamped;
    float value = 255.5f - scaled;
    int int_value = (int)value;
    int inverted = ~int_value;
    return (uint8_t)inverted;
}

// a x b in 2D
static inline msdfcc_float_t msdfcc_points_cross(msdfcc_point_t a, msdfcc_point_t b) noexcept {
    return a.x * b.y - a.y * b.x;
}

// a · b in 2D
static inline msdfcc_float_t msdfcc_points_dot(msdfcc_point_t a, msdfcc_point_t b) noexcept {
    return a.x * b.x + a.y * b.y;
}

enum msdfcc_edge_color {
    msdfcc_edge_color_black = 0,
    msdfcc_edge_color_red = 1,
    msdfcc_edge_color_green = 2,
    msdfcc_edge_color_yellow = 3,
    msdfcc_edge_color_blue = 4,
    msdfcc_edge_color_magenta = 5,
    msdfcc_edge_color_cyan = 6,
    msdfcc_edge_color_white = 7
};

enum msdfcc_edge_type {
    msdfcc_edge_type_none = 0,
    msdfcc_edge_type_linear,
    msdfcc_edge_type_quadratic,
    msdfcc_edge_type_cubic,
};

typedef struct {
    enum msdfcc_edge_type type;
    enum msdfcc_edge_color color;
    msdfcc_point_t p[4];
} msdfcc_edge_t ;


msdfcc_edge_t*
msdfcc_edge_linear(msdfcc_edge_t* out, msdfcc_point_t p0, msdfcc_point_t p1) noexcept;

msdfcc_edge_t*
msdfcc_edge_quadratic(msdfcc_edge_t* out, msdfcc_point_t p0, msdfcc_point_t p1, msdfcc_point_t p2) noexcept;

msdfcc_edge_t*
msdfcc_edge_cubic(msdfcc_edge_t* out, msdfcc_point_t p0, msdfcc_point_t p1, msdfcc_point_t p2, msdfcc_point_t p3) noexcept;

typedef struct {
    uint32_t length;
    uint32_t capacity;
    // FAM: c++ compatibility
    msdfcc_edge_t segments[1];
} msdfcc_contour_t;


msdfcc_contour_t*
msdfcc_contour_add_segment(msdfcc_contour_t*, const msdfcc_edge_t*) noexcept;

typedef struct {
    uint32_t length;
    uint32_t capacity;
    // FAM: c++ compatibility
    msdfcc_contour_t*   contours[1];
} msdfcc_shape_t;

msdfcc_shape_t*
msdfcc_shape_create(uint32_t) noexcept;

msdfcc_shape_t*
msdfcc_shape_new_contour(msdfcc_shape_t*) noexcept;

void
msdfcc_shape_normalize(msdfcc_shape_t*) noexcept;

void
msdfcc_edge_coloring_simple(msdfcc_shape_t* s, msdfcc_float_t angle_threshold, unsigned long long seed) noexcept;

void
msdfcc_shape_dispose(msdfcc_shape_t*) noexcept;


#define MSDFCC_ERROR_CORRECTION_DEFAULT_MIN_DEVIATION_RATIO 1.11111111111111111
#define MSDFCC_ERROR_CORRECTION_DEFAULT_MIN_IMPROVE_RATIO   1.11111111111111111

typedef enum {
    msdfcc_ec_mode_disabled,
    // Corrects all discontinuities regardless of edges.
    msdfcc_ec_mode_indiscriminate,
    // Corrects at edges only if it doesn't affect edges/corners. 
    msdfcc_ec_mode_edge_priority,
    // Only corrects artifacts at edges. 
    msdfcc_ec_mode_edge_only
} msdfcc_ec_mode_t;

typedef enum {
    // Never computes exact shape distance. 
    msdfcc_ec_distance_check_do_not,
    // Only at edges. Good balance. 
    msdfcc_ec_distance_check_at_edge,
    // Always compute and compare exact distance. Slower.
    msdfcc_ec_distance_check_always
} msdfcc_ec_distance_check_mode_t;

typedef struct {
    msdfcc_ec_mode_t mode;
    msdfcc_ec_distance_check_mode_t distance_check_mode;
    msdfcc_float_t min_deviation_ratio;
    msdfcc_float_t min_improve_ratio;
    // Optional stencil buffer. Must have >= width*height bytes. If NULL, allocated internally.
    unsigned char* buffer;
} msdfcc_error_correction_config_t;

// Initialize config with defaults. 
static inline msdfcc_error_correction_config_t
msdfcc_error_correction_config_default() noexcept {
    msdfcc_error_correction_config_t c;
    c.mode = msdfcc_ec_mode_edge_priority;
    c.distance_check_mode = msdfcc_ec_distance_check_at_edge;
    c.min_deviation_ratio = (msdfcc_float_t)MSDFCC_ERROR_CORRECTION_DEFAULT_MIN_DEVIATION_RATIO;
    c.min_improve_ratio = (msdfcc_float_t)MSDFCC_ERROR_CORRECTION_DEFAULT_MIN_IMPROVE_RATIO;
    c.buffer = 0;
    return c;
}

// --- SDF transformation (reference: SDFTransformation.h, Projection.h, Range.hpp, DistanceMapping.h) ---

typedef struct {
    msdfcc_point_t scale;
    msdfcc_point_t translate;
} msdfcc_projection_t;

typedef struct {
    msdfcc_float_t lower;
    msdfcc_float_t upper;
} msdfcc_range_t;

typedef struct {
    msdfcc_float_t scale;
    msdfcc_float_t translate;
} msdfcc_distance_mapping_t;

typedef struct {
    msdfcc_projection_t projection;
    msdfcc_distance_mapping_t distance_mapping;
} msdfcc_sdf_transformation_t;


void
msdfcc_generate_mtsdf_overlap(
    msdfcc_rgba_t* bitmap, msdfcc_size2d_t size,
    const msdfcc_shape_t* shape, const msdfcc_sdf_transformation_t* transformation,
    const msdfcc_error_correction_config_t* config) noexcept;

// reference: msdfgen msdfErrorCorrection. overlap_support: use overlap combiner when doing shape-based distance check. */
void
msdfcc_msdf_error_correction_overlap(
    msdfcc_rgba_t* bitmap, msdfcc_size2d_t size,
    const msdfcc_shape_t* shape, const msdfcc_sdf_transformation_t* transformation,
    const msdfcc_error_correction_config_t* config) noexcept;


// --- Distance sign correction (reference: rasterization.cpp distanceSignCorrection) ---

typedef enum {
    msdfcc_fill_nonzero,
    msdfcc_fill_odd,
    msdfcc_fill_positive,
    msdfcc_fill_negative
} msdfcc_fill_rule_t;

/** Fixes the sign of the signed distance field so it matches the shape fill(scanline pass).
 *  Uses shape projection; pass transformation->projection. sdf_zero_value typically 0.5f. */
void
msdfcc_distance_sign_correction(
    msdfcc_rgba_t* bitmap,
    msdfcc_size2d_t size,
    const msdfcc_shape_t* shape,
    const msdfcc_projection_t* projection,
    float sdf_zero_value,
    msdfcc_fill_rule_t fill_rule) noexcept;


static inline
msdfcc_range_t msdfcc_range_init1(msdfcc_float_t symmetricalWidth) noexcept {
    msdfcc_range_t rv = { MSDFCC_LIT(-0.5) * symmetricalWidth, MSDFCC_LIT(0.5) * symmetricalWidth };
    return rv;
}

// reference: SDFTransformation(Projection, Range), DistanceMapping(Range)
static inline msdfcc_sdf_transformation_t
msdfcc_sdf_transformation_init(msdfcc_projection_t projection, msdfcc_range_t range) noexcept {
    msdfcc_sdf_transformation_t out;
    out.projection = projection;
    {
        msdfcc_float_t range_width = range.upper - range.lower;
        out.distance_mapping.scale = (range_width != 0) ? (MSDFCC_LIT(1.0) / range_width) : MSDFCC_LIT(1.0);
        out.distance_mapping.translate = -range.lower;
    }
    return out;
}

#ifdef __cplusplus
}
#endif

#endif





#ifdef MSDFCC_C_IMPLEMENTATION
#include <math.h>
#include <assert.h>
#include <float.h>
#include <string.h>

#ifndef msdfcc_malloc
#define msdfcc_malloc malloc
#endif

#ifndef msdfcc_realloc
#define msdfcc_realloc realloc
#endif

#ifndef msdfcc_free
#define msdfcc_free free
#endif

#ifdef MSDFCC_USE_FLOAT
#define MSDFCC_CORNER_DOT_EPSILON .00001
#define MSDFCC_DECONVERGE_OVERSHOOT 1.11111111111111111
#define MSDFCC_SOLVE_QUADRATIC_THRESHOLD 1e8
#define MSDFCC_SOLVE_CUBIC_NORMED_EPSILON 1e-8
#define MSDFCC_SOLVE_CUBIC_THRESHOLD 5e4
#else
#define MSDFCC_CORNER_DOT_EPSILON .000001   // reference: Shape.h
#define MSDFCC_DECONVERGE_OVERSHOOT 1.11111111111111111  // reference: Shape.cpp
#define MSDFCC_SOLVE_QUADRATIC_THRESHOLD 1e12
#define MSDFCC_SOLVE_CUBIC_NORMED_EPSILON 1e-12
#define MSDFCC_SOLVE_CUBIC_THRESHOLD 1e6
#endif

/* Math macros for float/double when using msdfcc_float_t (no C11 _Generic). */
#ifdef MSDFCC_USE_FLOAT
#define msdfcc_abs(x)    fabsf(x)
#define msdfcc_sqrt(x)   sqrtf(x)
#define msdfcc_sin(x)    sinf(x)
#define msdfcc_cos(x)    cosf(x)
#define msdfcc_acos(x)   acosf(x)
#define msdfcc_pow(x,y)  powf(x,y)
#define msdfcc_floor(x)  floorf(x)
#define MSDFCC_UNIT_MAX  FLT_MAX
#else
#define msdfcc_abs(x)    fabs(x)
#define msdfcc_sqrt(x)   sqrt(x)
#define msdfcc_sin(x)    sin(x)
#define msdfcc_cos(x)    cos(x)
#define msdfcc_acos(x)   acos(x)
#define msdfcc_pow(x,y)  pow(x,y)
#define msdfcc_floor(x)  floor(x)
#define MSDFCC_UNIT_MAX  DBL_MAX
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
//   Shape: edgeCount (reference: msdfgen Shape::edgeCount)
// ----------------------------------------------------------------------------
uint32_t
msdfcc_shape_edge_count(const msdfcc_shape_t* s) noexcept {
    uint32_t total = 0;
    const uint32_t n = s->length;
    for (uint32_t i = 0; i < n; ++i) {
        const msdfcc_contour_t* c = s->contours[i];
        total += c->length;
    }
    return total;
}

// ----------------------------------------------------------------------------
//   MTSDF overlap contour combiner (ref: contour-combiners, edge-selectors)
// ----------------------------------------------------------------------------

typedef struct {
    msdfcc_float_t r, g, b, a;
} msdfcc_mtsdf_distance_t;

typedef struct {
    msdfcc_point_t point;
    msdfcc_float_t abs_distance;
    msdfcc_float_t a_domain_distance, b_domain_distance;
    msdfcc_float_t a_perpendicular_distance, b_perpendicular_distance;
} msdfcc_mtsdf_selector_edge_cache_t;

typedef struct {
    msdfcc_float_t distance;
    msdfcc_float_t dot;
} msdfcc_signed_distance_t;

typedef struct {
    msdfcc_signed_distance_t min_true_distance;
    msdfcc_float_t min_negative_perpendicular_distance;
    msdfcc_float_t min_positive_perpendicular_distance;
    // reference: PerpendicularDistanceSelectorBase::nearEdge */
    const msdfcc_edge_t* near_edge;
    msdfcc_float_t near_edge_param;
} msdfcc_perp_selector_t;

typedef struct {
    msdfcc_point_t p;
    msdfcc_perp_selector_t r, g, b;
} msdfcc_mtsdf_edge_selector_t;

/* C++ OverlappingContourCombiner has no has_previous_point; each EdgeSelector::reset(p) always does relax(delta) with delta = (p - this->p).length() * FACTOR (edge-selectors.cpp MultiDistanceSelector::reset). We mirror that: combiner->p initialized to (0,0), reset(p) always uses relax(delta). */
typedef struct {
    msdfcc_point_t p;
    uint32_t contour_count;
    int* windings;
    msdfcc_mtsdf_edge_selector_t* edge_selectors;
} msdfcc_mtsdf_overlap_contour_combiner_t;

static int
msdfcc_mtsdf_overlap_contour_combiner_init(msdfcc_mtsdf_overlap_contour_combiner_t* combiner, const msdfcc_shape_t* shape) noexcept;

static void
msdfcc_mtsdf_overlap_contour_combiner_uninit(msdfcc_mtsdf_overlap_contour_combiner_t* combiner) noexcept;

static void
msdfcc_mtsdf_overlap_contour_combiner_reset(msdfcc_mtsdf_overlap_contour_combiner_t* combiner, msdfcc_point_t p) noexcept;

msdfcc_mtsdf_distance_t
msdfcc_mtsdf_overlap_contour_combiner_distance(
    const msdfcc_mtsdf_overlap_contour_combiner_t* combiner,
    const msdfcc_shape_t* shape) noexcept;

// reference: EdgeSelector::addEdge(EdgeCache&, prevEdge, edge, nextEdge); no indices, uses edge pointer for near_edge. */
static void
msdfcc_mtsdf_selector_add_edge(
    msdfcc_mtsdf_edge_selector_t* selector,
    msdfcc_mtsdf_selector_edge_cache_t* cache,
    const msdfcc_edge_t* prev_edge, const msdfcc_edge_t* edge, const msdfcc_edge_t* next_edge) noexcept;

// reference: ShapeDistanceFinder::distance. edge_cache must have >= msdfcc_shape_edge_count(shape) elements (zero-initialized before first use). */
static msdfcc_mtsdf_distance_t
shape_distance_finder_overlap_mtsdf_distance(
    const msdfcc_shape_t* shape,
    msdfcc_mtsdf_overlap_contour_combiner_t* contour_combiner,
    msdfcc_mtsdf_selector_edge_cache_t* edge_cache,
    msdfcc_point_t origin) noexcept;

// For curves a, b converging at P = a->point(1) = b->point(0), returns ordering: -1, 0, or 1
static int
msdfcc_convergent_curve_ordering(const msdfcc_edge_t* a, const msdfcc_edge_t* b) noexcept;

enum msdfcc_constants {
    msdfcc_initial_capacity = 8
};

static inline float median3f(float a, float b, float c) noexcept {
    float mx = (a > b) ? a : b;
    float mn = (a < b) ? a : b;
    if (c > mx) return mx;
    if (c < mn) return mn;
    return c;
}

static inline float mixf(float a, float b, msdfcc_float_t t) noexcept {
    return (float)((MSDFCC_LIT(1.0) - t) * a + t * b);
}

static_assert(msdfcc_initial_capacity > 3, "msdfcc_initial_capacity must be > 3");


// ----------------------------------------------------------------------------
//                Helpers (from msdfgen arithmetics / Vector2)
// ----------------------------------------------------------------------------
static inline msdfcc_float_t cross_product(msdfcc_float_t ax, msdfcc_float_t ay, msdfcc_float_t bx, msdfcc_float_t by) noexcept {
    return ax * by - ay * bx;
}

static inline void vec_sub(msdfcc_float_t* ox, msdfcc_float_t* oy, msdfcc_float_t ax, msdfcc_float_t ay, msdfcc_float_t bx, msdfcc_float_t by) noexcept {
    *ox = ax - bx;
    *oy = ay - by;
}

static inline int points_equal(msdfcc_float_t x0, msdfcc_float_t y0, msdfcc_float_t x1, msdfcc_float_t y1) noexcept {
    return (x0 == x1 && y0 == y1);
}

// ----------------------------------------------------------------------------
//       Edge creation (reference: edge-segments.cpp EdgeSegment::create)
// ----------------------------------------------------------------------------

msdfcc_edge_t*
msdfcc_edge_linear(msdfcc_edge_t* out, msdfcc_point_t p0, msdfcc_point_t p1) noexcept {
    out->type = msdfcc_edge_type_linear;
    out->color = msdfcc_edge_color_white;
    out->p[0] = p0;
    out->p[1] = p1;
    return out;
}

msdfcc_edge_t*
msdfcc_edge_quadratic(msdfcc_edge_t* out, msdfcc_point_t p0, msdfcc_point_t p1, msdfcc_point_t p2) noexcept {
    // If (p1-p0) and (p2-p1) are colinear, collapse to linear p0->p2 (reference: crossProduct(p1-p0, p2-p1)==0)
    msdfcc_float_t p10x = p1.x - p0.x, p10y = p1.y - p0.y;
    msdfcc_float_t p21x = p2.x - p1.x, p21y = p2.y - p1.y;
    if (cross_product(p10x, p10y, p21x, p21y) == 0) {
        out->type = msdfcc_edge_type_linear;
        out->color = msdfcc_edge_color_white;
        out->p[0] = p0;
        out->p[1] = p2;
        return out;
    }
    out->type = msdfcc_edge_type_quadratic;
    out->color = msdfcc_edge_color_white;
    out->p[0] = p0;
    out->p[1] = p1;
    out->p[2] = p2;
    return out;
}

msdfcc_edge_t*
msdfcc_edge_cubic(msdfcc_edge_t* out, msdfcc_point_t p0, msdfcc_point_t p1, msdfcc_point_t p2, msdfcc_point_t p3) noexcept {
    msdfcc_float_t p10x = p1.x - p0.x, p10y = p1.y - p0.y;
    msdfcc_float_t p21x = p2.x - p1.x, p21y = p2.y - p1.y;
    msdfcc_float_t p32x = p3.x - p2.x, p32y = p3.y - p2.y;
    // Linear: both control legs colinear with chord
    if (cross_product(p10x, p10y, p21x, p21y) == 0 && cross_product(p21x, p21y, p32x, p32y) == 0) {
        out->type = msdfcc_edge_type_linear;
        out->color = msdfcc_edge_color_white;
        out->p[0] = p0;
        out->p[1] = p3;
        return out;
    }
    // Quadratic: 1.5*p1 - 0.5*p0 == 1.5*p2 - 0.5*p3 (converted control point)
    {
        msdfcc_float_t c1x = MSDFCC_LIT(1.5) * p1.x - MSDFCC_LIT(0.5) * p0.x, c1y = MSDFCC_LIT(1.5) * p1.y - MSDFCC_LIT(0.5) * p0.y;
        msdfcc_float_t c2x = MSDFCC_LIT(1.5) * p2.x - MSDFCC_LIT(0.5) * p3.x, c2y = MSDFCC_LIT(1.5) * p2.y - MSDFCC_LIT(0.5) * p3.y;
        if (c1x == c2x && c1y == c2y) {
            msdfcc_point_t q1 = { c1x, c1y };
            out->type = msdfcc_edge_type_quadratic;
            out->color = msdfcc_edge_color_white;
            out->p[0] = p0;
            out->p[1] = q1;
            out->p[2] = p3;
            return out;
        }
    }
    out->type = msdfcc_edge_type_cubic;
    out->color = msdfcc_edge_color_white;
    out->p[0] = p0;
    out->p[1] = p1;
    out->p[2] = p2;
    out->p[3] = p3;
    return out;
}

// mix(a, b, t) = (1-t)*a + t*b (reference: arithmetics.hpp)
static inline msdfcc_point_t
point_mix(msdfcc_point_t a, msdfcc_point_t b, msdfcc_float_t t) noexcept {
    msdfcc_point_t r;
    r.x = (MSDFCC_LIT(1.0) - t) * a.x + t * b.x;
    r.y = (MSDFCC_LIT(1.0) - t) * a.y + t * b.y;
    return r;
}

// Linear: point(t) = mix(p0, p1, t)
static inline msdfcc_point_t
linear_point(const msdfcc_point_t* p, msdfcc_float_t t) noexcept {
    return point_mix(p[0], p[1], t);
}

// Quadratic: point(t) = mix(mix(p0,p1,t), mix(p1,p2,t), t)
static inline msdfcc_point_t
quadratic_point(const msdfcc_point_t* p, msdfcc_float_t t) noexcept {
    return point_mix(point_mix(p[0], p[1], t), point_mix(p[1], p[2], t), t);
}

// Cubic: point(t) via De Casteljau
static inline msdfcc_point_t
cubic_point(const msdfcc_point_t* p, msdfcc_float_t t) noexcept {
    msdfcc_point_t p12 = point_mix(p[1], p[2], t);
    return point_mix(
        point_mix(point_mix(p[0], p[1], t), p12, t),
        point_mix(p12, point_mix(p[2], p[3], t), t),
        t);
}

// a - b
static inline msdfcc_point_t
point_sub(msdfcc_point_t a, msdfcc_point_t b) noexcept {
    msdfcc_point_t r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    return r;
}

static inline int
point_is_zero(msdfcc_point_t v) noexcept {
    return v.x == 0 && v.y == 0;
}

static inline msdfcc_float_t
point_length(msdfcc_point_t v) noexcept {
    return msdfcc_sqrt(v.x * v.x + v.y * v.y);
}

// reference: Vector2.hpp normalize; allow_zero: 0 = (0,1) for zero vec, 1 = (0,0)
static inline msdfcc_point_t
point_normalize(msdfcc_point_t v, int allow_zero) noexcept {
    msdfcc_float_t len = point_length(v);
    if (len > 0) {
        msdfcc_point_t r;
        r.x = v.x / len;
        r.y = v.y / len;
        return r;
    }
    msdfcc_point_t r;
    r.x = 0;
    r.y = allow_zero ? (msdfcc_float_t)0 : (msdfcc_float_t)1;
    return r;
}

// reference: Vector2.hpp getOrthogonal; polarity: 1 = (-y,x), 0 = (y,-x)
static inline msdfcc_point_t
point_get_orthogonal(msdfcc_point_t v, int polarity) noexcept {
    msdfcc_point_t r;
    if (polarity) {
        r.x = -v.y;
        r.y = v.x;
    }
    else {
        r.x = v.y;
        r.y = -v.x;
    }
    return r;
}

// reference: Shape.cpp deconvergeEdge; param 0 = start, 1 = end
static inline void
msdfcc_edge_deconverge(msdfcc_edge_t* edge, int param, msdfcc_point_t vector) noexcept {
    switch (edge->type) 
    {
    case msdfcc_edge_type_quadratic:
    {
        // convertToCubic: p0, mix(p0,p1,2/3), mix(p1,p2,1/3), p2 (edge-segments.cpp)
        msdfcc_point_t p0 = edge->p[0], p1 = edge->p[1], p2 = edge->p[2];
        edge->type = msdfcc_edge_type_cubic;
        edge->p[0] = p0;
        edge->p[1] = point_mix(p0, p1, MSDFCC_LIT(2.0/3.0));
        edge->p[2] = point_mix(p1, p2, MSDFCC_LIT(1.0/3.0));
        edge->p[3] = p2;
        // fallthrough
    }
    case msdfcc_edge_type_cubic:
    {
        msdfcc_point_t* p = edge->p;
        switch (param) 
        {
        case 0:
        {
            msdfcc_float_t len = point_length(point_sub(p[1], p[0]));
            p[1].x += len * vector.x;
            p[1].y += len * vector.y;
        }
        break;
        case 1:
        {
            msdfcc_float_t len = point_length(point_sub(p[2], p[3]));
            p[2].x += len * vector.x;
            p[2].y += len * vector.y;
        }
        break;
        }
        break;
    }
    case msdfcc_edge_type_linear:
    case msdfcc_edge_type_none:
    default:
        break;
    }
}

// reference: edge-segments.cpp LinearSegment/QuadraticSegment/CubicSegment::direction
static msdfcc_point_t
msdfcc_edge_direction(const msdfcc_edge_t* seg, msdfcc_float_t param) noexcept {
    const msdfcc_point_t* p = seg->p;
    switch (seg->type)
    {
    case msdfcc_edge_type_linear:
        return point_sub(p[1], p[0]);
    case msdfcc_edge_type_quadratic:
    {
        msdfcc_point_t d01 = point_sub(p[1], p[0]);
        msdfcc_point_t d12 = point_sub(p[2], p[1]);
        msdfcc_point_t tangent = point_mix(d01, d12, param);
        if (point_is_zero(tangent))
            return point_sub(p[2], p[0]);
        return tangent;
    }
    case msdfcc_edge_type_cubic:
    {
        msdfcc_point_t d01 = point_sub(p[1], p[0]);
        msdfcc_point_t d12 = point_sub(p[2], p[1]);
        msdfcc_point_t d23 = point_sub(p[3], p[2]);
        msdfcc_point_t tangent = point_mix(point_mix(d01, d12, param), point_mix(d12, d23, param), param);
        if (point_is_zero(tangent)) {
            if (param == 0)
                return point_sub(p[2], p[0]);
            if (param == 1)
                return point_sub(p[3], p[1]);
        }
        return tangent;
    }
    case msdfcc_edge_type_none:
    default:
    {
        assert(!"unknown");
        msdfcc_point_t zero = { 0, 0 };
        return zero;
    }
    }
}

// reference: arithmetics.hpp sign
static inline int sign_d(msdfcc_float_t n) noexcept {
    return (0 < n) - (n < 0);
}

// reference: convergent-curve-ordering.cpp simplifyDegenerateCurve
static inline void
simplify_degenerate_curve(msdfcc_point_t* cp, int* order) noexcept {
    if (*order == 3 && (msdfcc_points_equal(cp[1], cp[0]) || msdfcc_points_equal(cp[1], cp[3])) && (msdfcc_points_equal(cp[2], cp[0]) || msdfcc_points_equal(cp[2], cp[3]))) {
        cp[1] = cp[3];
        *order = 1;
    }
    if (*order == 2 && (msdfcc_points_equal(cp[1], cp[0]) || msdfcc_points_equal(cp[1], cp[2]))) {
        cp[1] = cp[2];
        *order = 1;
    }
    if (*order == 1 && msdfcc_points_equal(cp[0], cp[1]))
        *order = 0;
}

// reference: convergent-curve-ordering.cpp convergentCurveOrdering(Point2*, int, int)
static int
convergent_curve_ordering_impl(const msdfcc_point_t* corner, int controlPointsBefore, int controlPointsAfter) noexcept {
    if (!(controlPointsBefore > 0 && controlPointsAfter > 0))
        return 0;
    msdfcc_point_t a1, a2, a3, b1, b2, b3;
    a1 = point_sub(corner[-1], corner[0]);
    b1 = point_sub(corner[1], corner[0]);
    a2.x = a2.y = 0; a3.x = a3.y = 0;
    b2.x = b2.y = 0; b3.x = b3.y = 0;
    if (controlPointsBefore >= 2) {
        msdfcc_point_t m1 = point_sub(corner[-2], corner[-1]);
        a2 = point_sub(m1, a1);
    }
    if (controlPointsAfter >= 2) {
        msdfcc_point_t m1 = point_sub(corner[2], corner[1]);
        b2 = point_sub(m1, b1);
    }
    if (controlPointsBefore >= 3) {
        a3 = point_sub(point_sub(corner[-3], corner[-2]), point_sub(corner[-2], corner[-1]));
        a3 = point_sub(a3, a2);
        a2.x *= 3; a2.y *= 3;
    }
    if (controlPointsAfter >= 3) {
        b3 = point_sub(point_sub(corner[3], corner[2]), point_sub(corner[2], corner[1]));
        b3 = point_sub(b3, b2);
        b2.x *= 3; b2.y *= 3;
    }
    a1.x *= (msdfcc_float_t)controlPointsBefore; a1.y *= (msdfcc_float_t)controlPointsBefore;
    b1.x *= (msdfcc_float_t)controlPointsAfter; b1.y *= (msdfcc_float_t)controlPointsAfter;
    // Non-degenerate case
    if (!point_is_zero(a1) && !point_is_zero(b1)) {
        msdfcc_float_t as = point_length(a1);
        msdfcc_float_t bs = point_length(b1);
        msdfcc_float_t d;
        d = as * msdfcc_points_cross(a1, b2) + bs * msdfcc_points_cross(a2, b1);
        if (d != 0) return sign_d(d);
        d = as * as * msdfcc_points_cross(a1, b3) + as * bs * msdfcc_points_cross(a2, b2) + bs * bs * msdfcc_points_cross(a3, b1);
        if (d != 0) return sign_d(d);
        d = as * msdfcc_points_cross(a2, b3) + bs * msdfcc_points_cross(a3, b2);
        if (d != 0) return sign_d(d);
        return sign_d(msdfcc_points_cross(a3, b3));
    }
    // Degenerate: swap a<->b when a1 nonzero, b1 zero
    {
        int s = 1;
        if (!point_is_zero(a1)) {
            msdfcc_point_t t;
            b1 = a1;
            t = a1; a1 = b2; b2 = a2; a2 = t;
            t = a1; a1 = b3; b3 = a3; a3 = t;
            s = -1;
        }
        if (!point_is_zero(b1)) {
            msdfcc_float_t d;
            d = msdfcc_points_cross(a3, b1);
            if (d != 0) return s * sign_d(d);
            d = msdfcc_points_cross(a2, b2);
            if (d != 0) return s * sign_d(d);
            d = msdfcc_points_cross(a3, b2);
            if (d != 0) return s * sign_d(d);
            d = msdfcc_points_cross(a2, b3);
            if (d != 0) return s * sign_d(d);
            return s * sign_d(msdfcc_points_cross(a3, b3));
        }
        // Both degenerate
        {
            msdfcc_float_t d = msdfcc_sqrt(point_length(a2)) * msdfcc_points_cross(a2, b3) + msdfcc_sqrt(point_length(b2)) * msdfcc_points_cross(a3, b2);
            if (d != 0) return sign_d(d);
            return sign_d(msdfcc_points_cross(a3, b3));
        }
    }
}

// reference: convergent-curve-ordering.cpp convergentCurveOrdering(EdgeSegment*, EdgeSegment*)
static int
msdfcc_convergent_curve_ordering(const msdfcc_edge_t* a, const msdfcc_edge_t* b) noexcept {
    msdfcc_point_t controlPoints[12];
    msdfcc_point_t* corner = controlPoints + 4;
    msdfcc_point_t* aCpTmp = controlPoints + 8;
    int aOrder = (int)a->type;
    int bOrder = (int)b->type;
    if (aOrder < 1 || aOrder > 3 || bOrder < 1 || bOrder > 3)
        return 0;
    for (int i = 0; i <= aOrder; ++i)
        aCpTmp[i] = a->p[i];
    for (int i = 0; i <= bOrder; ++i)
        corner[i] = b->p[i];
    if (!msdfcc_points_equal(aCpTmp[aOrder], corner[0]))
        return 0;
    simplify_degenerate_curve(aCpTmp, &aOrder);
    simplify_degenerate_curve(corner, &bOrder);
    for (int i = 0; i < aOrder; ++i)
        corner[i - aOrder] = aCpTmp[i];
    return convergent_curve_ordering_impl(corner, aOrder, bOrder);
}

// reference: edge-segments.cpp EdgeSegment::splitInThirds
static void
msdfcc_edge_split_in_thirds(const msdfcc_edge_t* seg,
    msdfcc_edge_t* seg0, msdfcc_edge_t* seg1, msdfcc_edge_t* seg2) noexcept {

    const msdfcc_point_t* p = seg->p;
    enum msdfcc_edge_color col = seg->color;

    switch (seg->type)
    {
    case msdfcc_edge_type_linear:
    {
        msdfcc_point_t q13 = linear_point(p, MSDFCC_LIT(1.0/3.0));
        msdfcc_point_t q23 = linear_point(p, MSDFCC_LIT(2.0/3.0));
        msdfcc_edge_linear(seg0, p[0], q13);
        msdfcc_edge_linear(seg1, q13, q23);
        msdfcc_edge_linear(seg2, q23, p[1]);
        seg0->color = seg1->color = seg2->color = col;
        break;
    }
    case msdfcc_edge_type_quadratic:
    {
        msdfcc_point_t q13 = quadratic_point(p, MSDFCC_LIT(1.0/3.0));
        msdfcc_point_t q23 = quadratic_point(p, MSDFCC_LIT(2.0/3.0));
        msdfcc_point_t c0 = point_mix(p[0], p[1], MSDFCC_LIT(1.0/3.0));
        msdfcc_point_t c1 = point_mix(point_mix(p[0], p[1], MSDFCC_LIT(5.0/9.0)), point_mix(p[1], p[2], MSDFCC_LIT(4.0/9.0)), MSDFCC_LIT(0.5));
        msdfcc_point_t c2 = point_mix(p[1], p[2], MSDFCC_LIT(2.0/3.0));
        msdfcc_edge_quadratic(seg0, p[0], c0, q13);
        msdfcc_edge_quadratic(seg1, q13, c1, q23);
        msdfcc_edge_quadratic(seg2, q23, c2, p[2]);
        seg0->color = seg1->color = seg2->color = col;
        break;
    }
    case msdfcc_edge_type_cubic:
    {
        msdfcc_point_t q13 = cubic_point(p, MSDFCC_LIT(1.0/3.0));
        msdfcc_point_t q23 = cubic_point(p, MSDFCC_LIT(2.0/3.0));
        // seg0: p0, c01, c02, q13
        msdfcc_point_t c01 = msdfcc_points_equal(p[0], p[1]) ? p[0] : point_mix(p[0], p[1], MSDFCC_LIT(1.0/3.0));
        msdfcc_point_t c02 = point_mix(point_mix(p[0], p[1], MSDFCC_LIT(1.0/3.0)), point_mix(p[1], p[2], MSDFCC_LIT(1.0/3.0)), MSDFCC_LIT(1.0/3.0));
        // seg1: q13, c11, c12, q23
        msdfcc_point_t c11 = point_mix(
            point_mix(point_mix(p[0], p[1], MSDFCC_LIT(1.0/3.0)), point_mix(p[1], p[2], MSDFCC_LIT(1.0/3.0)), MSDFCC_LIT(1.0/3.0)),
            point_mix(point_mix(p[1], p[2], MSDFCC_LIT(1.0/3.0)), point_mix(p[2], p[3], MSDFCC_LIT(1.0/3.0)), MSDFCC_LIT(1.0/3.0)),
            MSDFCC_LIT(2.0/3.0));
        msdfcc_point_t c12 = point_mix(
            point_mix(point_mix(p[0], p[1], MSDFCC_LIT(2.0/3.0)), point_mix(p[1], p[2], MSDFCC_LIT(2.0/3.0)), MSDFCC_LIT(2.0/3.0)),
            point_mix(point_mix(p[1], p[2], MSDFCC_LIT(2.0/3.0)), point_mix(p[2], p[3], MSDFCC_LIT(2.0/3.0)), MSDFCC_LIT(2.0/3.0)),
            MSDFCC_LIT(1.0/3.0));
        // seg2: q23, c21, c22, p3
        msdfcc_point_t c21 = point_mix(point_mix(p[1], p[2], MSDFCC_LIT(2.0/3.0)), point_mix(p[2], p[3], MSDFCC_LIT(2.0/3.0)), MSDFCC_LIT(2.0/3.0));
        msdfcc_point_t c22 = msdfcc_points_equal(p[2], p[3]) ? p[3] : point_mix(p[2], p[3], MSDFCC_LIT(2.0/3.0));
        msdfcc_edge_cubic(seg0, p[0], c01, c02, q13);
        msdfcc_edge_cubic(seg1, q13, c11, c12, q23);
        msdfcc_edge_cubic(seg2, q23, c21, c22, p[3]);
        seg0->color = seg1->color = seg2->color = col;
        break;
    }
    case msdfcc_edge_type_none:
    default:
        assert(!"unknown");
        // no-op for unknown/none
        break;
    }
}


// ----------------------------------------------------------------------------
//   Contour: add segment with realloc, double on grow, fail = return original
// ----------------------------------------------------------------------------

static inline size_t contour_alloc_size(uint32_t capacity) noexcept {
    return offsetof(msdfcc_contour_t, segments) + (size_t)capacity * sizeof(msdfcc_edge_t);
}

msdfcc_contour_t*
msdfcc_contour_add_segment(msdfcc_contour_t* c, const msdfcc_edge_t* seg) noexcept {
    if (!seg) return c;
    if (!c) {
        size_t size = contour_alloc_size(msdfcc_initial_capacity);
        msdfcc_contour_t* c0 = (msdfcc_contour_t*)msdfcc_malloc(size);
        if (!c0) return NULL;
        c0->length = 1;
        c0->capacity = msdfcc_initial_capacity;
        c0->segments[0] = *seg;
        return c0;
    }
    if (c->length >= c->capacity) {
        uint32_t new_cap = c->capacity * 2;
        size_t new_size = contour_alloc_size(new_cap);
        msdfcc_contour_t* c1 = (msdfcc_contour_t*)msdfcc_realloc(c, new_size);
        if (!c1) return c;
        c = c1;
        c->capacity = new_cap;
    }
    c->segments[c->length] = *seg;
    c->length++;
    return c;
}

// ----------------------------------------------------------------------------
//   Shape: create, new contour (new_contour does not accept nullptr)
// ----------------------------------------------------------------------------

static inline size_t shape_alloc_size(uint32_t capacity) noexcept {
    return offsetof(msdfcc_shape_t, contours) + (size_t)capacity * sizeof(msdfcc_contour_t*);
}

msdfcc_shape_t*
msdfcc_shape_create(uint32_t capacity) noexcept {
    ++capacity;
    if (capacity < (uint32_t)msdfcc_initial_capacity)
        capacity = (uint32_t)msdfcc_initial_capacity;
    size_t size = shape_alloc_size(capacity);
    msdfcc_shape_t* s = (msdfcc_shape_t*)msdfcc_malloc(size);
    if (!s) return NULL;
    s->length = 0;
    s->capacity = capacity;
    return s;
}

msdfcc_shape_t*
msdfcc_shape_new_contour(msdfcc_shape_t* s) noexcept {
    if (!s)
        return NULL;
    // Does not accept nullptr: s must be non-NULL. Length += 1; expand when not enough.
    if ((s->length + 1) >= s->capacity) {
        uint32_t new_cap = s->capacity * 2;
        size_t new_size = shape_alloc_size(new_cap);
        msdfcc_shape_t* s1 = (msdfcc_shape_t*)msdfcc_realloc(s, new_size);
        if (!s1) return s;
        s = s1;
        s->capacity = new_cap;
    }
    s->contours[s->length] = NULL;
    s->length++;
    return s;
}

// ----------------------------------------------------------------------------
//   Shape: normalize (reference: Shape.cpp Shape::normalize)
// ----------------------------------------------------------------------------

void
msdfcc_shape_normalize(msdfcc_shape_t* s) noexcept {
    if (!s) return;
    const uint32_t n = s->length;
    for (uint32_t ci = 0; ci < n; ++ci) {
        msdfcc_contour_t* c = s->contours[ci];
        assert(c && c->capacity >= (uint32_t)msdfcc_initial_capacity);
        const uint32_t m = c->length;
        if (m == 1) {
            assert(c->capacity >= 3);
            // splitInThirds: replace single edge with 3 segments
            msdfcc_edge_t seg0, seg1, seg2;
            msdfcc_edge_split_in_thirds(&c->segments[0], &seg0, &seg1, &seg2);
            c->segments[0] = seg0;
            c->segments[1] = seg1;
            c->segments[2] = seg2;
            c->length = 3;
        }
        else if (m > 1) {
            // Push apart convergent edge segments
            const msdfcc_float_t dot_eps = (msdfcc_float_t)(MSDFCC_CORNER_DOT_EPSILON - 1.0);
            const msdfcc_float_t factor = (msdfcc_float_t)(MSDFCC_DECONVERGE_OVERSHOOT * msdfcc_sqrt(MSDFCC_LIT(1.0) - dot_eps * dot_eps) / dot_eps);
            for (uint32_t i = 0; i < m; ++i) {
                uint32_t prev_i = (i == 0) ? m - 1 : i - 1;
                msdfcc_edge_t* prev_edge = &c->segments[prev_i];
                msdfcc_edge_t* edge = &c->segments[i];
                msdfcc_point_t prev_dir = msdfcc_edge_direction(prev_edge, MSDFCC_LIT(1.0));
                msdfcc_point_t cur_dir = msdfcc_edge_direction(edge, MSDFCC_LIT(0.0));
                prev_dir = point_normalize(prev_dir, 0);
                cur_dir = point_normalize(cur_dir, 0);
                if (msdfcc_points_dot(prev_dir, cur_dir) < dot_eps) {
                    msdfcc_point_t diff = point_sub(cur_dir, prev_dir);
                    msdfcc_point_t axis = point_normalize(diff, 0);
                    axis.x *= factor;
                    axis.y *= factor;
                    if (msdfcc_convergent_curve_ordering(prev_edge, edge) < 0) {
                        axis.x = -axis.x;
                        axis.y = -axis.y;
                    }
                    msdfcc_edge_deconverge(prev_edge, 1, point_get_orthogonal(axis, 1));
                    msdfcc_edge_deconverge(edge, 0, point_get_orthogonal(axis, 0));
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
//   Edge coloring (reference: edge-coloring.cpp edgeColoringSimple)
// ----------------------------------------------------------------------------

// symmetricalTrichotomy: for position in [0,n-1], returns -1, 0, or 1
static inline int symmetrical_trichotomy(int position, int n) noexcept {
    return (int)(3 + MSDFCC_LIT(2.875) * (msdfcc_float_t)position / (n - 1) - MSDFCC_LIT(1.4375) + MSDFCC_LIT(0.5)) - 3;
}

static inline int is_corner(msdfcc_point_t a_dir, msdfcc_point_t b_dir, msdfcc_float_t cross_threshold) noexcept {
    return msdfcc_points_dot(a_dir, b_dir) <= 0 || msdfcc_abs(msdfcc_points_cross(a_dir, b_dir)) > cross_threshold;
}

static inline int seed_extract2(unsigned long long* seed) noexcept {
    int v = (int)(*seed & 1);
    *seed >>= 1;
    return v;
}

static inline int seed_extract3(unsigned long long* seed) noexcept {
    int v = (int)(*seed % 3);
    *seed /= 3;
    return v;
}

static enum msdfcc_edge_color init_color(unsigned long long* seed) noexcept {
    static const enum msdfcc_edge_color colors[3] = {
        msdfcc_edge_color_cyan, msdfcc_edge_color_magenta, msdfcc_edge_color_yellow
    };
    return colors[seed_extract3(seed)];
}

static void switch_color(enum msdfcc_edge_color* color, unsigned long long* seed) noexcept {
    int shifted = (int)*color << (1 + seed_extract2(seed));
    *color = (enum msdfcc_edge_color)((shifted | (shifted >> 3)) & (int)msdfcc_edge_color_white);
}

static void switch_color_banned(enum msdfcc_edge_color* color, unsigned long long* seed, enum msdfcc_edge_color banned) noexcept {
    int combined = (int)(*color & banned);
    if (combined == (int)msdfcc_edge_color_red || combined == (int)msdfcc_edge_color_green || combined == (int)msdfcc_edge_color_blue)
        *color = (enum msdfcc_edge_color)(combined ^ (int)msdfcc_edge_color_white);
    else
        switch_color(color, seed);
}

void
msdfcc_edge_coloring_simple(msdfcc_shape_t* s, msdfcc_float_t angle_threshold, unsigned long long seed) noexcept {
    if (!s) return;
    const msdfcc_float_t cross_threshold = msdfcc_sin(angle_threshold);

    // Precompute max contour length for corners buffer
    uint32_t max_corners = 0;
    const uint32_t n_contours = s->length;
    for (uint32_t ci = 0; ci < n_contours; ++ci) {
        const msdfcc_contour_t* c = s->contours[ci];
        assert(c);
        if (c->length > max_corners)
            max_corners = c->length;
    }
    if (max_corners == 0) return;

    int* const corners = (int*)msdfcc_malloc((size_t)max_corners * sizeof(int));
    if (!corners) return;

    enum msdfcc_edge_color color = init_color(&seed);

    for (uint32_t ci = 0; ci < n_contours; ++ci) {
        msdfcc_contour_t* c = s->contours[ci];
        if (c->length == 0) continue;

        const uint32_t m = c->length;

        // Identify corners
        int corner_count = 0;
        msdfcc_point_t prev_dir = msdfcc_edge_direction(&c->segments[m - 1], MSDFCC_LIT(1.0));
        prev_dir = point_normalize(prev_dir, 0);
        for (uint32_t ei = 0; ei < m; ++ei) {
            msdfcc_point_t cur_dir = msdfcc_edge_direction(&c->segments[ei], MSDFCC_LIT(0.0));
            cur_dir = point_normalize(cur_dir, 0);
            if (is_corner(prev_dir, cur_dir, cross_threshold))
                corners[corner_count++] = (int)ei;
            prev_dir = msdfcc_edge_direction(&c->segments[ei], MSDFCC_LIT(1.0));
            prev_dir = point_normalize(prev_dir, 0);
        }

        // Smooth contour
        if (corner_count == 0) {
            switch_color(&color, &seed);
            for (uint32_t ei = 0; ei < m; ++ei)
                c->segments[ei].color = color;
        }
        // Teardrop case
        else if (corner_count == 1) {
            enum msdfcc_edge_color colors[3];
            switch_color(&color, &seed);
            colors[0] = color;
            colors[1] = msdfcc_edge_color_white;
            switch_color(&color, &seed);
            colors[2] = color;
            const int corner = corners[0];

            if (m >= 3) {
                for (uint32_t i = 0; i < m; ++i) {
                    const int idx = (corner + (int)i) % (int)m;
                    c->segments[idx].color = colors[1 + symmetrical_trichotomy((int)i, (int)m)];
                }
            }
            else {
                // Split edges: need 3 or 6 segments
                assert(m >= 1 && c->capacity >= 6);
                msdfcc_edge_t parts[6];
                msdfcc_edge_split_in_thirds(&c->segments[0], &parts[0 + 3 * corner], &parts[1 + 3 * corner], &parts[2 + 3 * corner]);
                if (m >= 2) {
                    msdfcc_edge_split_in_thirds(&c->segments[1], &parts[3 - 3 * corner], &parts[4 - 3 * corner], &parts[5 - 3 * corner]);
                    parts[0].color = parts[1].color = colors[0];
                    parts[2].color = parts[3].color = colors[1];
                    parts[4].color = parts[5].color = colors[2];
                    for (int i = 0; i < 6; ++i)
                        c->segments[i] = parts[i];
                    c->length = 6;
                }
                else {
                    parts[0].color = colors[0];
                    parts[1].color = colors[1];
                    parts[2].color = colors[2];
                    c->segments[0] = parts[0];
                    c->segments[1] = parts[1];
                    c->segments[2] = parts[2];
                    c->length = 3;
                }
            }
        }
        // Multiple corners
        else {
            const int corner_count_i = corner_count;
            int spline = 0;
            const int start = corners[0];
            switch_color(&color, &seed);
            enum msdfcc_edge_color initial_color = color;
            for (uint32_t i = 0; i < m; ++i) {
                const int index = (start + (int)i) % (int)m;
                if (spline + 1 < corner_count_i && corners[spline + 1] == index) {
                    ++spline;
                    switch_color_banned(&color, &seed, (enum msdfcc_edge_color)((spline == corner_count_i - 1) * (int)initial_color));
                }
                c->segments[index].color = color;
            }
        }
    }

    msdfcc_free(corners);
}

// ----------------------------------------------------------------------------
//   Shape: dispose shape and each contour (contours are pointers)
// ----------------------------------------------------------------------------

void
msdfcc_shape_dispose(msdfcc_shape_t* s) noexcept {
    if (!s) return;
    for (uint32_t i = 0; i < s->length; ++i)
        msdfcc_free(s->contours[i]);
    msdfcc_free(s);
}


// ----------------------------------------------------------------------------
//   generator (reference: contour-combiners, edge-selectors, MultiAndTrueDistanceSelector)
// ----------------------------------------------------------------------------
// types: msdfcc.h

// reference: Contour.cpp shoelace, winding
static inline msdfcc_float_t shoelace(msdfcc_point_t a, msdfcc_point_t b) noexcept {
    return (b.x - a.x) * (a.y + b.y);
}

static msdfcc_point_t edge_point(const msdfcc_edge_t* seg, msdfcc_float_t t) noexcept {
    const msdfcc_point_t* p = seg->p;
    switch (seg->type) 
    {
    case msdfcc_edge_type_linear:  return linear_point(p, t);
    case msdfcc_edge_type_quadratic: return quadratic_point(p, t);
    case msdfcc_edge_type_cubic:   return cubic_point(p, t);
    default: return p[0];
    }
}

// reference: arithmetics.hpp nonZeroSign; edge-selectors.cpp DISTANCE_DELTA_FACTOR
#define MSDFCC_DISTANCE_DELTA_FACTOR 1.001

static inline int non_zero_sign(msdfcc_float_t n) noexcept {
    return 2 * (n > 0) - 1;
}

// reference: equation-solver.cpp solveQuadratic
static int solve_quadratic(msdfcc_float_t x[2], msdfcc_float_t a, msdfcc_float_t b, msdfcc_float_t c) noexcept {
    if (a == 0 || msdfcc_abs(b) > MSDFCC_LIT(MSDFCC_SOLVE_QUADRATIC_THRESHOLD) * msdfcc_abs(a)) {
        if (b == 0)
            return (c == 0) ? -1 : 0;
        x[0] = -c / b;
        return 1;
    }
    {
        msdfcc_float_t dscr = b * b - 4 * a * c;
        if (dscr > 0) {
            dscr = msdfcc_sqrt(dscr);
            x[0] = (-b + dscr) / (2 * a);
            x[1] = (-b - dscr) / (2 * a);
            return 2;
        }
        else if (dscr == 0) {
            x[0] = -b / (2 * a);
            return 1;
        }
    }
    return 0;
}

// reference: equation-solver.cpp solveCubicNormed, solveCubic
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static int solve_cubic_normed(msdfcc_float_t x[3], msdfcc_float_t a, msdfcc_float_t b, msdfcc_float_t c) noexcept {
    msdfcc_float_t a2 = a * a;
    msdfcc_float_t q = MSDFCC_LIT(1.0/9.0) * (a2 - 3 * b);
    msdfcc_float_t r = MSDFCC_LIT(1.0/54.0) * (a * (2 * a2 - 9 * b) + 27 * c);
    msdfcc_float_t r2 = r * r;
    msdfcc_float_t q3 = q * q * q;
    a *= MSDFCC_LIT(1.0/3.0);
    if (r2 < q3) {
        msdfcc_float_t t = r / msdfcc_sqrt(q3);
        if (t < -1) t = MSDFCC_LIT(-1);
        if (t > 1) t = MSDFCC_LIT(1);
        t = msdfcc_acos(t);
        q = -2 * msdfcc_sqrt(q);
        x[0] = q * msdfcc_cos(MSDFCC_LIT(1.0/3.0) * t) - a;
        x[1] = q * msdfcc_cos(MSDFCC_LIT(1.0/3.0) * (t + 2 * (msdfcc_float_t)M_PI)) - a;
        x[2] = q * msdfcc_cos(MSDFCC_LIT(1.0/3.0) * (t - 2 * (msdfcc_float_t)M_PI)) - a;
        return 3;
    }
    else {
        msdfcc_float_t u = (r < 0 ? 1 : -1) * msdfcc_pow(msdfcc_abs(r) + msdfcc_sqrt(r2 - q3), MSDFCC_LIT(1.0/3.0));
        msdfcc_float_t v = (u == 0) ? 0 : q / u;
        x[0] = (u + v) - a;
        if (u == v || msdfcc_abs(u - v) < MSDFCC_LIT(MSDFCC_SOLVE_CUBIC_NORMED_EPSILON) * msdfcc_abs(u + v)) {
            x[1] = MSDFCC_LIT(-0.5) * (u + v) - a;
            return 2;
        }
        return 1;
    }
}

static int solve_cubic(msdfcc_float_t x[3], msdfcc_float_t a, msdfcc_float_t b, msdfcc_float_t c, msdfcc_float_t d) noexcept {
    if (a != 0 && msdfcc_abs(b / a) < MSDFCC_LIT(MSDFCC_SOLVE_CUBIC_THRESHOLD))
        return solve_cubic_normed(x, b / a, c / a, d / a);
    return solve_quadratic(x, b, c, d);
}

// ----------------------------------------------------------------------------
//   Scanline for distance sign correction (reference: rasterization.cpp, Scanline.cpp, edge-segments.cpp)
// ----------------------------------------------------------------------------

// reference: LinearSegment::scanlineIntersections
static int edge_scanline_intersections_linear(const msdfcc_point_t* p, msdfcc_float_t y, msdfcc_float_t x_out[3], int dy_out[3]) noexcept {
    if ((y >= p[0].y && y < p[1].y) || (y >= p[1].y && y < p[0].y)) {
        msdfcc_float_t param = (p[1].y != p[0].y) ? (y - p[0].y) / (p[1].y - p[0].y) : 0;
        x_out[0] = (MSDFCC_LIT(1.0) - param) * p[0].x + param * p[1].x;
        dy_out[0] = sign_d(p[1].y - p[0].y);
        return 1;
    }
    return 0;
}

// reference: QuadraticSegment::scanlineIntersections
static int edge_scanline_intersections_quadratic(const msdfcc_point_t* p, msdfcc_float_t y, msdfcc_float_t x_out[3], int dy_out[3]) noexcept {
    int total = 0;
    int next_dy = (y > p[0].y) ? 1 : -1;
    x_out[total] = p[0].x;
    if (p[0].y == y) {
        if (p[0].y < p[1].y || (p[0].y == p[1].y && p[0].y < p[2].y))
            dy_out[total++] = 1;
        else
            next_dy = 1;
    }
    {
        msdfcc_float_t abx = p[1].x - p[0].x, aby = p[1].y - p[0].y;
        msdfcc_float_t brx = (p[2].x - p[1].x) - abx, bry = (p[2].y - p[1].y) - aby;
        msdfcc_float_t t[2];
        int solutions = solve_quadratic(t, bry, 2 * aby, p[0].y - y);
        if (solutions >= 2 && t[0] > t[1]) {
            msdfcc_float_t tmp = t[0]; t[0] = t[1]; t[1] = tmp;
        }
        for (int i = 0; i < solutions && total < 2; ++i) {
            if (t[i] >= 0 && t[i] <= 1) {
                x_out[total] = p[0].x + 2 * t[i] * abx + t[i] * t[i] * brx;
                if (next_dy * (aby + t[i] * bry) >= 0) {
                    dy_out[total++] = next_dy;
                    next_dy = -next_dy;
                }
            }
        }
    }
    if (p[2].y == y) {
        if (next_dy > 0 && total > 0) {
            --total;
            next_dy = -1;
        }
        if ((p[2].y < p[1].y || (p[2].y == p[1].y && p[2].y < p[0].y)) && total < 2) {
            x_out[total] = p[2].x;
            if (next_dy < 0) {
                dy_out[total++] = -1;
                next_dy = 1;
            }
        }
    }
    if (next_dy != (y >= p[2].y ? 1 : -1)) {
        if (total > 0)
            --total;
        else {
            if (msdfcc_abs(p[2].y - y) < msdfcc_abs(p[0].y - y))
                x_out[total] = p[2].x;
            dy_out[total++] = next_dy;
        }
    }
    return total;
}

// reference: CubicSegment::scanlineIntersections
static int edge_scanline_intersections_cubic(const msdfcc_point_t* p, msdfcc_float_t y, msdfcc_float_t x_out[3], int dy_out[3]) noexcept {
    int total = 0;
    int next_dy = (y > p[0].y) ? 1 : -1;
    x_out[total] = p[0].x;
    if (p[0].y == y) {
        if (p[0].y < p[1].y || (p[0].y == p[1].y && (p[0].y < p[2].y || (p[0].y == p[2].y && p[0].y < p[3].y))))
            dy_out[total++] = 1;
        else
            next_dy = 1;
    }
    {
        msdfcc_float_t abx = p[1].x - p[0].x, aby = p[1].y - p[0].y;
        msdfcc_float_t brx = (p[2].x - p[1].x) - abx, bry = (p[2].y - p[1].y) - aby;
        msdfcc_float_t asx = (p[3].x - p[2].x) - (p[2].x - p[1].x) - brx;
        msdfcc_float_t asy = (p[3].y - p[2].y) - (p[2].y - p[1].y) - bry;
        msdfcc_float_t t[3];
        int solutions = solve_cubic(t, asy, 3 * bry, 3 * aby, p[0].y - y);
        if (solutions >= 2) {
            if (t[0] > t[1]) { msdfcc_float_t tmp = t[0]; t[0] = t[1]; t[1] = tmp; }
            if (solutions >= 3 && t[1] > t[2]) {
                msdfcc_float_t tmp = t[1]; t[1] = t[2]; t[2] = tmp;
                if (t[0] > t[1]) { tmp = t[0]; t[0] = t[1]; t[1] = tmp; }
            }
        }
        for (int i = 0; i < solutions && total < 3; ++i) {
            if (t[i] >= 0 && t[i] <= 1) {
                x_out[total] = p[0].x + 3 * t[i] * abx + 3 * t[i] * t[i] * brx + t[i] * t[i] * t[i] * asx;
                if (next_dy * (aby + 2 * t[i] * bry + t[i] * t[i] * asy) >= 0) {
                    dy_out[total++] = next_dy;
                    next_dy = -next_dy;
                }
            }
        }
    }
    if (p[3].y == y) {
        if (next_dy > 0 && total > 0) {
            --total;
            next_dy = -1;
        }
        if ((p[3].y < p[2].y || (p[3].y == p[2].y && (p[3].y < p[1].y || (p[3].y == p[1].y && p[3].y < p[0].y)))) && total < 3) {
            x_out[total] = p[3].x;
            if (next_dy < 0) {
                dy_out[total++] = -1;
                next_dy = 1;
            }
        }
    }
    if (next_dy != (y >= p[3].y ? 1 : -1)) {
        if (total > 0)
            --total;
        else {
            if (msdfcc_abs(p[3].y - y) < msdfcc_abs(p[0].y - y))
                x_out[total] = p[3].x;
            dy_out[total++] = next_dy;
        }
    }
    return total;
}

typedef struct { msdfcc_float_t x; int direction; } scanline_intersection_t;

//static int compare_intersections(const void* a, const void* b) noexcept {
//    msdfcc_float_t d = ((const scanline_intersection_t*)a)->x - ((const scanline_intersection_t*)b)->x;
//    return (d > 0) ? 1 : ((d < 0) ? -1 : 0);
//}

static void
qsort_intersections(scanline_intersection_t* arr, int n) noexcept;

// reference: Shape::scanline - collect all edge intersections at y, sort by x, make direction cumulative
static uint32_t shape_scanline_at_y(
    const msdfcc_shape_t* shape, msdfcc_float_t y,
    scanline_intersection_t* out, uint32_t out_cap) noexcept {
    uint32_t n = 0;
    msdfcc_float_t xx[3];
    int dy[3];
    const uint32_t n_contours = shape->length;
    for (uint32_t ci = 0; ci < n_contours && n < out_cap; ++ci) {
        const msdfcc_contour_t* c = shape->contours[ci];
        for (uint32_t ei = 0; ei < c->length && n < out_cap; ++ei) {
            const msdfcc_edge_t* e = &c->segments[ei];
            int count = 0;
            switch (e->type) 
            {
            case msdfcc_edge_type_linear:
                count = edge_scanline_intersections_linear(e->p, y, xx, dy);
                break;
            case msdfcc_edge_type_quadratic:
                count = edge_scanline_intersections_quadratic(e->p, y, xx, dy);
                break;
            case msdfcc_edge_type_cubic:
                count = edge_scanline_intersections_cubic(e->p, y, xx, dy);
                break;
            default:
                break;
            }
            for (int i = 0; i < count && n < out_cap; ++i) {
                out[n].x = xx[i];
                out[n].direction = dy[i];
                ++n;
            }
        }
    }
    if (n == 0) return 0;
    //qsort(out, n, sizeof(scanline_intersection_t), compare_intersections);
    qsort_intersections(out, n);
    // preprocess: direction becomes cumulative (reference: Scanline::preprocess)
    int total = 0;
    for (uint32_t i = 0; i < n; ++i) {
        total += out[i].direction;
        out[i].direction = total;
    }
    return n;
}

// reference: interpretFillRule
static int interpret_fill_rule(int sum, msdfcc_fill_rule_t fill_rule) noexcept {
    switch (fill_rule) 
    {
    case msdfcc_fill_nonzero: return sum != 0;
    case msdfcc_fill_odd:     return (sum & 1) != 0;
    case msdfcc_fill_positive: return sum > 0;
    case msdfcc_fill_negative: return sum < 0;
    }
    return 0;
}

// reference: Scanline::filled - sum at x is the cumulative direction at last intersection <= x
static int scanline_sum_at_x(const scanline_intersection_t* line, uint32_t n, msdfcc_float_t x) noexcept {
    if (n == 0) return 0;
    int index = -1;
    for (uint32_t i = 0; i < n; ++i) {
        if (line[i].x <= x)
            index = (int)i;
        else
            break;
    }
    return (index >= 0) ? line[index].direction : 0;
}

// reference: edge-segments.cpp LinearSegment::signedDistance
static msdfcc_signed_distance_t edge_signed_distance_linear(
    const msdfcc_point_t* p, msdfcc_point_t origin, msdfcc_float_t* param_out) noexcept {
    msdfcc_point_t aq = point_sub(origin, p[0]);
    msdfcc_point_t ab = point_sub(p[1], p[0]);
    msdfcc_float_t ab_dot = msdfcc_points_dot(ab, ab);
    msdfcc_float_t t = (ab_dot != 0) ? msdfcc_points_dot(aq, ab) / ab_dot : 0;
    msdfcc_point_t eq = point_sub(p[t > MSDFCC_LIT(0.5) ? 1 : 0], origin);
    msdfcc_float_t endpoint_dist = point_length(eq);
    if (t > 0 && t < 1) {
        msdfcc_point_t ortho = point_get_orthogonal(ab, 0);
        ortho = point_normalize(ortho, 1);
        msdfcc_float_t ortho_dist = msdfcc_points_dot(ortho, aq);
        if (msdfcc_abs(ortho_dist) < endpoint_dist) {
            *param_out = t;
            msdfcc_signed_distance_t rv = { ortho_dist, 0 };
            return rv;
        }
    }
    *param_out = t;
    {
        msdfcc_point_t abn = point_normalize(ab, 1);
        msdfcc_point_t eqn = point_normalize(eq, 1);
        msdfcc_float_t dot_val = msdfcc_abs(msdfcc_points_dot(abn, eqn));
        msdfcc_float_t dist = (msdfcc_float_t)non_zero_sign(msdfcc_points_cross(aq, ab)) * endpoint_dist;
        msdfcc_signed_distance_t rv = { dist, dot_val };
        return rv;
    }
}

// reference: edge-segments.cpp QuadraticSegment::signedDistance
static msdfcc_signed_distance_t edge_signed_distance_quadratic(
    const msdfcc_point_t* p, msdfcc_point_t origin, msdfcc_float_t* param_out) noexcept {
    msdfcc_point_t qa = point_sub(p[0], origin);
    msdfcc_point_t ab = point_sub(p[1], p[0]);
    msdfcc_point_t br = point_sub(point_sub(p[2], p[1]), ab);
    msdfcc_float_t a = msdfcc_points_dot(br, br);
    msdfcc_float_t b = 3 * msdfcc_points_dot(ab, br);
    msdfcc_float_t c = 2 * msdfcc_points_dot(ab, ab) + msdfcc_points_dot(qa, br);
    msdfcc_float_t d = msdfcc_points_dot(qa, ab);
    msdfcc_float_t t[3];
    int solutions = solve_cubic(t, a, b, c, d);

    msdfcc_point_t ep_dir = ab;  // direction(0)
    msdfcc_float_t min_dist = (msdfcc_float_t)non_zero_sign(msdfcc_points_cross(ep_dir, qa)) * point_length(qa);
    msdfcc_float_t param = -msdfcc_points_dot(qa, ep_dir) / msdfcc_points_dot(ep_dir, ep_dir);
    {
        msdfcc_float_t dist_b = point_length(point_sub(p[2], origin));
        if (dist_b < msdfcc_abs(min_dist)) {
            ep_dir = point_sub(p[2], p[1]);
            min_dist = (msdfcc_float_t)non_zero_sign(msdfcc_points_cross(ep_dir, point_sub(p[2], origin))) * dist_b;
            param = msdfcc_points_dot(point_sub(origin, p[1]), ep_dir) / msdfcc_points_dot(ep_dir, ep_dir);
        }
    }
    for (int i = 0; i < solutions; ++i) {
        if (t[i] > 0 && t[i] < 1) {
            msdfcc_point_t qe;
            qe.x = qa.x + 2 * t[i] * ab.x + t[i] * t[i] * br.x;
            qe.y = qa.y + 2 * t[i] * ab.y + t[i] * t[i] * br.y;
            msdfcc_float_t dist = point_length(qe);
            if (dist <= msdfcc_abs(min_dist)) {
                msdfcc_point_t tangent;
                tangent.x = ab.x + t[i] * br.x;
                tangent.y = ab.y + t[i] * br.y;
                min_dist = (msdfcc_float_t)non_zero_sign(msdfcc_points_cross(tangent, qe)) * dist;
                param = t[i];
            }
        }
    }
    *param_out = param;
    if (param >= 0 && param <= 1) {
        msdfcc_signed_distance_t rv = { min_dist, 0 };
        return rv;
    }
    if (param < MSDFCC_LIT(0.5)) {
        msdfcc_point_t d0 = point_sub(p[1], p[0]);
        d0 = point_normalize(d0, 1);
        msdfcc_point_t qan = point_normalize(qa, 1);
        msdfcc_signed_distance_t rv = { min_dist, msdfcc_abs(msdfcc_points_dot(d0, qan)) };
        return rv;
    }
    {
        msdfcc_point_t d1 = point_sub(p[2], p[1]);
        d1 = point_normalize(d1, 1);
        msdfcc_point_t p2o = point_sub(p[2], origin);
        p2o = point_normalize(p2o, 1);
        msdfcc_signed_distance_t rv = { min_dist, msdfcc_abs(msdfcc_points_dot(d1, p2o)) };
        return rv;
    }
}

#define MSDFCC_CUBIC_SEARCH_STARTS 4
#define MSDFCC_CUBIC_SEARCH_STEPS 4

// reference: edge-segments.cpp CubicSegment::signedDistance
msdfcc_signed_distance_t
edge_signed_distance_cubic(const msdfcc_point_t* p, msdfcc_point_t origin, msdfcc_float_t* param_out) noexcept {
    msdfcc_point_t qa = point_sub(p[0], origin);
    msdfcc_point_t ab = point_sub(p[1], p[0]);
    msdfcc_point_t br = point_sub(p[2], p[1]);
    br.x -= ab.x; br.y -= ab.y;
    msdfcc_point_t as = point_sub(point_sub(p[3], p[2]), point_sub(p[2], p[1]));
    as.x -= br.x; as.y -= br.y;

    msdfcc_point_t ep_dir = point_sub(p[1], p[0]);
    msdfcc_float_t min_dist = (msdfcc_float_t)non_zero_sign(msdfcc_points_cross(ep_dir, qa)) * point_length(qa);
    msdfcc_float_t param = -msdfcc_points_dot(qa, ep_dir) / msdfcc_points_dot(ep_dir, ep_dir);
    {
        msdfcc_point_t p3o = point_sub(p[3], origin);
        msdfcc_float_t dist_b = point_length(p3o);
        if (dist_b < msdfcc_abs(min_dist)) {
            // Use direction(1) instead of p[3]-p[2] to match msdfgen behavior
            msdfcc_point_t d12 = point_sub(p[2], p[1]);
            msdfcc_point_t d23 = point_sub(p[3], p[2]);
            ep_dir = point_mix(d12, d23, MSDFCC_LIT(1.0));
            if (point_is_zero(ep_dir)) {
                ep_dir = point_sub(p[3], p[1]);
            }
            min_dist = (msdfcc_float_t)non_zero_sign(msdfcc_points_cross(ep_dir, p3o)) * dist_b;
            param = msdfcc_points_dot(point_sub(ep_dir, p3o), ep_dir) / msdfcc_points_dot(ep_dir, ep_dir);
        }
    }
    for (int i = 0; i <= MSDFCC_CUBIC_SEARCH_STARTS; ++i) {
        msdfcc_float_t t = (msdfcc_float_t)i / (msdfcc_float_t)MSDFCC_CUBIC_SEARCH_STARTS;
        msdfcc_point_t qe, d1, d2;
        qe.x = qa.x + 3 * t * ab.x + 3 * t * t * br.x + t * t * t * as.x;
        qe.y = qa.y + 3 * t * ab.y + 3 * t * t * br.y + t * t * t * as.y;
        d1.x = 3 * ab.x + 6 * t * br.x + 3 * t * t * as.x;
        d1.y = 3 * ab.y + 6 * t * br.y + 3 * t * t * as.y;
        d2.x = 6 * br.x + 6 * t * as.x;
        d2.y = 6 * br.y + 6 * t * as.y;
        msdfcc_float_t improved_t = t - msdfcc_points_dot(qe, d1) / (msdfcc_points_dot(d1, d1) + msdfcc_points_dot(qe, d2));
        if (improved_t > 0 && improved_t < 1) {
            int steps = MSDFCC_CUBIC_SEARCH_STEPS;
            do {
                t = improved_t;
                qe.x = qa.x + 3 * t * ab.x + 3 * t * t * br.x + t * t * t * as.x;
                qe.y = qa.y + 3 * t * ab.y + 3 * t * t * br.y + t * t * t * as.y;
                d1.x = 3 * ab.x + 6 * t * br.x + 3 * t * t * as.x;
                d1.y = 3 * ab.y + 6 * t * br.y + 3 * t * t * as.y;
                if (--steps == 0) break;
                d2.x = 6 * br.x + 6 * t * as.x;
                d2.y = 6 * br.y + 6 * t * as.y;
                improved_t = t - msdfcc_points_dot(qe, d1) / (msdfcc_points_dot(d1, d1) + msdfcc_points_dot(qe, d2));
            } while (improved_t > 0 && improved_t < 1);
            msdfcc_float_t dist = point_length(qe);
            if (dist < msdfcc_abs(min_dist)) {
                min_dist = (msdfcc_float_t)non_zero_sign(msdfcc_points_cross(d1, qe)) * dist;
                param = t;
            }
        }
    }
    *param_out = param;
    if (param >= 0 && param <= 1) {
        msdfcc_signed_distance_t rv = { min_dist, 0 };
        return rv;
    }
    if (param < MSDFCC_LIT(0.5)) {
        msdfcc_point_t d0 = point_sub(p[1], p[0]);
        d0 = point_normalize(d0, 1);
        msdfcc_point_t qan = point_normalize(qa, 1);
        msdfcc_signed_distance_t rv = { min_dist, msdfcc_abs(msdfcc_points_dot(d0, qan)) };
        return rv;
    }
    {
        // Use direction(1) instead of p[3]-p[2] to match msdfgen behavior
        msdfcc_point_t d12 = point_sub(p[2], p[1]);
        msdfcc_point_t d23 = point_sub(p[3], p[2]);
        msdfcc_point_t d1 = point_mix(d12, d23, MSDFCC_LIT(1.0));
        if (point_is_zero(d1)) {
            d1 = point_sub(p[3], p[1]);
        }
        d1 = point_normalize(d1, 1);
        msdfcc_point_t p3o = point_sub(p[3], origin);
        p3o = point_normalize(p3o, 1);
        msdfcc_signed_distance_t rv = { min_dist, msdfcc_abs(msdfcc_points_dot(d1, p3o)) };
        return rv;
    }
}

static msdfcc_signed_distance_t edge_signed_distance(
    const msdfcc_edge_t* edge, msdfcc_point_t origin, msdfcc_float_t* param_out) noexcept {
    switch (edge->type)
    {
    case msdfcc_edge_type_linear:
        return edge_signed_distance_linear(edge->p, origin, param_out);
    case msdfcc_edge_type_quadratic:
        return edge_signed_distance_quadratic(edge->p, origin, param_out);
    case msdfcc_edge_type_cubic:
        return edge_signed_distance_cubic(edge->p, origin, param_out);
    default:
        *param_out = 0;
        msdfcc_signed_distance_t rv = { -MSDFCC_UNIT_MAX, 0 };
        return rv;
    }
}

static int contour_winding(const msdfcc_contour_t* c) noexcept {
    if (!c || c->length == 0) return 0;
    const uint32_t m = c->length;
    msdfcc_float_t total = 0;
    if (m == 1) {
        msdfcc_point_t a = edge_point(&c->segments[0], MSDFCC_LIT(0));
        msdfcc_point_t b = edge_point(&c->segments[0], MSDFCC_LIT(1.0/3.0));
        msdfcc_point_t c_pt = edge_point(&c->segments[0], MSDFCC_LIT(2.0/3.0));
        total += shoelace(a, b) + shoelace(b, c_pt) + shoelace(c_pt, a);
    }
    else if (m == 2) {
        msdfcc_point_t a = edge_point(&c->segments[0], MSDFCC_LIT(0)), b = edge_point(&c->segments[0], MSDFCC_LIT(0.5));
        msdfcc_point_t c_pt = edge_point(&c->segments[1], MSDFCC_LIT(0)), d = edge_point(&c->segments[1], MSDFCC_LIT(0.5));
        total += shoelace(a, b) + shoelace(b, c_pt) + shoelace(c_pt, d) + shoelace(d, a);
    }
    else {
        msdfcc_point_t prev = edge_point(&c->segments[m - 1], MSDFCC_LIT(0));
        for (uint32_t i = 0; i < m; ++i) {
            msdfcc_point_t cur = edge_point(&c->segments[i], MSDFCC_LIT(0));
            total += shoelace(prev, cur);
            prev = cur;
        }
    }
    return (total > 0) - (total < 0);
}

// Single allocation: windings then edge_selectors; windings is the base pointer for free.
int
msdfcc_mtsdf_overlap_contour_combiner_init(msdfcc_mtsdf_overlap_contour_combiner_t* combiner, const msdfcc_shape_t* shape) noexcept {
    assert(combiner);
    if (!shape) return 0;
    const uint32_t n = shape->length;
    if (n == 0) {
        combiner->contour_count = 0;
        combiner->windings = NULL;
        combiner->edge_selectors = NULL;
        combiner->p.x = 0;
        combiner->p.y = 0;
        return 1;
    }
    size_t selectors_size = (size_t)n * sizeof(msdfcc_mtsdf_edge_selector_t);
    size_t windings_size = (size_t)n * sizeof(int);
    void* block = msdfcc_malloc(selectors_size + windings_size);
    if (!block) return 0;
    combiner->edge_selectors = (msdfcc_mtsdf_edge_selector_t*)block;
    combiner->windings = (int*)((char*)block + selectors_size);
    combiner->contour_count = n;
    combiner->p.x = 0;
    combiner->p.y = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (shape->contours[i])
            combiner->windings[i] = contour_winding(shape->contours[i]);
        else
            combiner->windings[i] = 0;
    }
    return 1;
}

void
msdfcc_mtsdf_overlap_contour_combiner_uninit(msdfcc_mtsdf_overlap_contour_combiner_t* combiner) noexcept {
    assert(combiner);
    msdfcc_free(combiner->edge_selectors);  // base of single malloc block
    combiner->edge_selectors = NULL;
    combiner->windings = NULL;
    combiner->contour_count = 0;
}

// reference: arithmetics.hpp median
static inline msdfcc_float_t median3(msdfcc_float_t a, msdfcc_float_t b, msdfcc_float_t c) noexcept {
    msdfcc_float_t mx = (a > b) ? a : b;
    msdfcc_float_t mn = (a < b) ? a : b;
    if (c > mx) return mx;
    if (c < mn) return mn;
    return c;
}

// reference: contour-combiners.cpp initDistance(MultiAndTrueDistance)
static inline void init_mtsdf_distance(msdfcc_mtsdf_distance_t* d) noexcept {
    d->r = d->g = d->b = d->a = -MSDFCC_UNIT_MAX;
}

// reference: contour-combiners.cpp resolveDistance(MultiDistance) -> median(r,g,b)
static inline msdfcc_float_t resolve_mtsdf_distance(const msdfcc_mtsdf_distance_t* d) noexcept {
    return median3(d->r, d->g, d->b);
}

// reference: SignedDistance.hpp operator<
static inline int signed_distance_less(msdfcc_float_t ad, msdfcc_float_t adot, msdfcc_float_t bd, msdfcc_float_t bdot) noexcept {
    msdfcc_float_t aa = msdfcc_abs(ad), bb = msdfcc_abs(bd);
    return (aa < bb) || (aa == bb && adot < bdot);
}

// reference: edge-segments.cpp EdgeSegment::distanceToPerpendicularDistance
static void edge_distance_to_perpendicular_distance(
    const msdfcc_edge_t* edge, msdfcc_point_t origin, msdfcc_float_t param,
    msdfcc_float_t* distance_out, msdfcc_float_t* dot_out) noexcept {
    msdfcc_float_t dist = *distance_out;
    if (param < 0) {
        msdfcc_point_t dir = msdfcc_edge_direction(edge, MSDFCC_LIT(0.0));
        dir = point_normalize(dir, 1);
        msdfcc_point_t pt0 = edge_point(edge, MSDFCC_LIT(0));
        msdfcc_point_t aq = point_sub(origin, pt0);
        msdfcc_float_t ts = msdfcc_points_dot(aq, dir);
        if (ts < 0) {
            msdfcc_float_t perpendicular_distance = msdfcc_points_cross(aq, dir);
            if (msdfcc_abs(perpendicular_distance) <= msdfcc_abs(dist)) {
                *distance_out = perpendicular_distance;
                *dot_out = 0;
            }
        }
    }
    else if (param > 1) {
        msdfcc_point_t dir = msdfcc_edge_direction(edge, MSDFCC_LIT(1.0));
        dir = point_normalize(dir, 1);
        msdfcc_point_t pt1 = edge_point(edge, MSDFCC_LIT(1.0));
        msdfcc_point_t bq = point_sub(origin, pt1);
        msdfcc_float_t ts = msdfcc_points_dot(bq, dir);
        if (ts > 0) {
            msdfcc_float_t perpendicular_distance = msdfcc_points_cross(bq, dir);
            if (msdfcc_abs(perpendicular_distance) <= msdfcc_abs(dist)) {
                *distance_out = perpendicular_distance;
                *dot_out = 0;
            }
        }
    }
}

// reference: PerpendicularDistanceSelectorBase::computeDistance - uses nearEdge pointer
static msdfcc_float_t perp_selector_compute_distance(
    const msdfcc_perp_selector_t* ch, msdfcc_point_t p) noexcept {
    msdfcc_float_t min_dist = (ch->min_true_distance.distance < 0)
        ? ch->min_negative_perpendicular_distance
        : ch->min_positive_perpendicular_distance;
    if (ch->near_edge) {
        msdfcc_float_t dist = ch->min_true_distance.distance;
        msdfcc_float_t dot_val = ch->min_true_distance.dot;
        edge_distance_to_perpendicular_distance(
            ch->near_edge, p, ch->near_edge_param, &dist, &dot_val);
        if (msdfcc_abs(dist) < msdfcc_abs(min_dist))
            min_dist = dist;
    }
    return min_dist;
}

// reference: MultiDistanceSelector::distance, MultiAndTrueDistanceSelector::distance
static msdfcc_mtsdf_distance_t mtsdf_selector_distance(
    const msdfcc_mtsdf_edge_selector_t* sel, msdfcc_point_t p,
    const msdfcc_shape_t* shape) noexcept {
    (void)shape;
    msdfcc_mtsdf_distance_t out;
    out.r = perp_selector_compute_distance(&sel->r, p);
    out.g = perp_selector_compute_distance(&sel->g, p);
    out.b = perp_selector_compute_distance(&sel->b, p);
    // trueDistance = min of r,g,b true distances (reference: MultiDistanceSelector::trueDistance)
    {
        msdfcc_float_t best_d = sel->r.min_true_distance.distance;
        msdfcc_float_t best_dot = sel->r.min_true_distance.dot;
        if (signed_distance_less(sel->g.min_true_distance.distance, sel->g.min_true_distance.dot, best_d, best_dot)) {
            best_d = sel->g.min_true_distance.distance;
            best_dot = sel->g.min_true_distance.dot;
        }
        if (signed_distance_less(sel->b.min_true_distance.distance, sel->b.min_true_distance.dot, best_d, best_dot))
            best_d = sel->b.min_true_distance.distance;
        out.a = best_d;
    }
    return out;
}

// reference: PerpendicularDistanceSelectorBase::merge
static void perp_selector_merge(msdfcc_perp_selector_t* dest, const msdfcc_perp_selector_t* src) noexcept {
    if (signed_distance_less(src->min_true_distance.distance, src->min_true_distance.dot,
        dest->min_true_distance.distance, dest->min_true_distance.dot)) {
        dest->min_true_distance = src->min_true_distance;
        dest->near_edge = src->near_edge;
        dest->near_edge_param = src->near_edge_param;
    }
    if (src->min_negative_perpendicular_distance > dest->min_negative_perpendicular_distance)
        dest->min_negative_perpendicular_distance = src->min_negative_perpendicular_distance;
    if (src->min_positive_perpendicular_distance < dest->min_positive_perpendicular_distance)
        dest->min_positive_perpendicular_distance = src->min_positive_perpendicular_distance;
}

// reference: MultiDistanceSelector::merge
static void mtsdf_selector_merge(msdfcc_mtsdf_edge_selector_t* dest, const msdfcc_mtsdf_edge_selector_t* src) noexcept {
    perp_selector_merge(&dest->r, &src->r);
    perp_selector_merge(&dest->g, &src->g);
    perp_selector_merge(&dest->b, &src->b);
}


// reference: PerpendicularDistanceSelectorBase::getPerpendicularDistance
static bool get_perpendicular_distance(
    msdfcc_float_t* distance, msdfcc_point_t ep, msdfcc_point_t edge_dir) noexcept {
    const msdfcc_float_t ts = msdfcc_points_dot(ep, edge_dir);
    if (ts > 0) {
        const msdfcc_float_t perpendicular_distance = msdfcc_points_cross(ep, edge_dir);
        if (msdfcc_abs(perpendicular_distance) < msdfcc_abs(*distance)) {
            *distance = perpendicular_distance;
            return true;
        }
    }
    return false;
}

// reference: PerpendicularDistanceSelectorBase::isEdgeRelevant
static bool perp_selector_is_edge_relevant(
    const msdfcc_perp_selector_t* ch, const msdfcc_mtsdf_selector_edge_cache_t* cache,
    msdfcc_point_t p) noexcept {
    msdfcc_float_t delta = (msdfcc_float_t)MSDFCC_DISTANCE_DELTA_FACTOR * point_length(point_sub(p, cache->point));
    return (
        (cache->abs_distance - delta <= msdfcc_abs(ch->min_true_distance.distance)) ||
        (msdfcc_abs(cache->a_domain_distance) < delta) ||
        (msdfcc_abs(cache->b_domain_distance) < delta) ||
        (cache->a_domain_distance > 0 && (
            cache->a_perpendicular_distance < 0 ?
            cache->a_perpendicular_distance + delta >= ch->min_negative_perpendicular_distance :
            cache->a_perpendicular_distance - delta <= ch->min_positive_perpendicular_distance
            )) ||
        (cache->b_domain_distance > 0 && (
            cache->b_perpendicular_distance < 0 ?
            cache->b_perpendicular_distance + delta >= ch->min_negative_perpendicular_distance :
            cache->b_perpendicular_distance - delta <= ch->min_positive_perpendicular_distance
            ))
        );
}

// reference: PerpendicularDistanceSelectorBase::addEdgeTrueDistance(edge, distance, param)
static void perp_selector_add_edge_true_distance(
    msdfcc_perp_selector_t* ch, const msdfcc_signed_distance_t* distance, msdfcc_float_t param,
    const msdfcc_edge_t* edge) noexcept {
    if (signed_distance_less(distance->distance, distance->dot,
        ch->min_true_distance.distance, ch->min_true_distance.dot)) {
        ch->min_true_distance = *distance;
        ch->near_edge = edge;
        ch->near_edge_param = param;
    }
}

// reference: PerpendicularDistanceSelectorBase::addEdgePerpendicularDistance
static void perp_selector_add_edge_perpendicular_distance(
    msdfcc_perp_selector_t* ch, msdfcc_float_t distance) noexcept {
    if (distance <= 0 && distance > ch->min_negative_perpendicular_distance)
        ch->min_negative_perpendicular_distance = distance;
    if (distance >= 0 && distance < ch->min_positive_perpendicular_distance)
        ch->min_positive_perpendicular_distance = distance;
}

// reference: MultiDistanceSelector::addEdge - stores edge pointer as near_edge
void
msdfcc_mtsdf_selector_add_edge(
    msdfcc_mtsdf_edge_selector_t* selector,
    msdfcc_mtsdf_selector_edge_cache_t* cache,
    const msdfcc_edge_t* prev_edge, const msdfcc_edge_t* edge, const msdfcc_edge_t* next_edge) noexcept {
    msdfcc_point_t p = selector->p;
    int relevant = (
        ((edge->color & (int)msdfcc_edge_color_red) && perp_selector_is_edge_relevant(&selector->r, cache, p)) ||
        ((edge->color & (int)msdfcc_edge_color_green) && perp_selector_is_edge_relevant(&selector->g, cache, p)) ||
        ((edge->color & (int)msdfcc_edge_color_blue) && perp_selector_is_edge_relevant(&selector->b, cache, p))
        );
    if (!relevant) return;

    msdfcc_float_t param;
    msdfcc_signed_distance_t dist = edge_signed_distance(edge, p, &param);

    if (edge->color & (int)msdfcc_edge_color_red)
        perp_selector_add_edge_true_distance(&selector->r, &dist, param, edge);
    if (edge->color & (int)msdfcc_edge_color_green)
        perp_selector_add_edge_true_distance(&selector->g, &dist, param, edge);
    if (edge->color & (int)msdfcc_edge_color_blue)
        perp_selector_add_edge_true_distance(&selector->b, &dist, param, edge);

    cache->point = p;
    cache->abs_distance = msdfcc_abs(dist.distance);

    msdfcc_point_t pt0 = edge_point(edge, MSDFCC_LIT(0));
    msdfcc_point_t pt1 = edge_point(edge, MSDFCC_LIT(1.0));
    msdfcc_point_t ap = point_sub(p, pt0);
    msdfcc_point_t bp = point_sub(p, pt1);
    msdfcc_point_t a_dir = msdfcc_edge_direction(edge, MSDFCC_LIT(0.0));
    a_dir = point_normalize(a_dir, 1);
    msdfcc_point_t b_dir = msdfcc_edge_direction(edge, MSDFCC_LIT(1.0));
    b_dir = point_normalize(b_dir, 1);
    msdfcc_point_t prev_dir = msdfcc_edge_direction(prev_edge, MSDFCC_LIT(1.0));
    prev_dir = point_normalize(prev_dir, 1);
    msdfcc_point_t next_dir = msdfcc_edge_direction(next_edge, MSDFCC_LIT(0.0));
    next_dir = point_normalize(next_dir, 1);

    msdfcc_point_t prev_plus_a;
    prev_plus_a.x = prev_dir.x + a_dir.x;
    prev_plus_a.y = prev_dir.y + a_dir.y;
    prev_plus_a = point_normalize(prev_plus_a, 1);
    msdfcc_float_t add_val = msdfcc_points_dot(ap, prev_plus_a);

    msdfcc_point_t b_plus_next;
    b_plus_next.x = b_dir.x + next_dir.x;
    b_plus_next.y = b_dir.y + next_dir.y;
    b_plus_next = point_normalize(b_plus_next, 1);
    msdfcc_float_t bdd = -msdfcc_points_dot(bp, b_plus_next);

    if (add_val > 0) {
        msdfcc_point_t neg_a_dir = { -a_dir.x, -a_dir.y };
        msdfcc_float_t pd = dist.distance;
        if (get_perpendicular_distance(&pd, ap, neg_a_dir)) {
            pd = -pd;
            if (edge->color & (int)msdfcc_edge_color_red)
                perp_selector_add_edge_perpendicular_distance(&selector->r, pd);
            if (edge->color & (int)msdfcc_edge_color_green)
                perp_selector_add_edge_perpendicular_distance(&selector->g, pd);
            if (edge->color & (int)msdfcc_edge_color_blue)
                perp_selector_add_edge_perpendicular_distance(&selector->b, pd);
        }
        cache->a_perpendicular_distance = pd;
    }
    if (bdd > 0) {
        msdfcc_float_t pd = dist.distance;
        if (get_perpendicular_distance(&pd, bp, b_dir)) {
            if (edge->color & (int)msdfcc_edge_color_red)
                perp_selector_add_edge_perpendicular_distance(&selector->r, pd);
            if (edge->color & (int)msdfcc_edge_color_green)
                perp_selector_add_edge_perpendicular_distance(&selector->g, pd);
            if (edge->color & (int)msdfcc_edge_color_blue)
                perp_selector_add_edge_perpendicular_distance(&selector->b, pd);
        }
        cache->b_perpendicular_distance = pd;
    }
    cache->a_domain_distance = add_val;
    cache->b_domain_distance = bdd;
}

// reference: PerpendicularDistanceSelectorBase reset; MultiDistanceSelector::reset
static inline void perp_selector_reset(msdfcc_perp_selector_t* ch) noexcept {
    ch->min_true_distance.distance = -MSDFCC_UNIT_MAX;
    ch->min_true_distance.dot = 0;
    ch->min_negative_perpendicular_distance = -MSDFCC_UNIT_MAX;
    ch->min_positive_perpendicular_distance = MSDFCC_UNIT_MAX;
    ch->near_edge = NULL;
    ch->near_edge_param = 0;
}

// reference: PerpendicularDistanceSelectorBase::reset(delta) - relax previous min by delta for spatial coherence
static inline void perp_selector_relax(msdfcc_perp_selector_t* ch, msdfcc_float_t delta) noexcept {
    msdfcc_float_t d = ch->min_true_distance.distance;
    d += (msdfcc_float_t)non_zero_sign(d) * delta;
    ch->min_true_distance.distance = d;
    ch->min_true_distance.dot = 0;
    ch->min_negative_perpendicular_distance = -msdfcc_abs(d);
    ch->min_positive_perpendicular_distance = msdfcc_abs(d);
    ch->near_edge = NULL;
    ch->near_edge_param = 0;
}

static inline void mtsdf_edge_selector_reset(msdfcc_mtsdf_edge_selector_t* sel, msdfcc_point_t p) noexcept {
    sel->p = p;
    perp_selector_reset(&sel->r);
    perp_selector_reset(&sel->g);
    perp_selector_reset(&sel->b);
}

static inline void mtsdf_edge_selector_relax(msdfcc_mtsdf_edge_selector_t* sel, msdfcc_point_t p, msdfcc_float_t delta) noexcept {
    sel->p = p;
    perp_selector_relax(&sel->r, delta);
    perp_selector_relax(&sel->g, delta);
    perp_selector_relax(&sel->b, delta);
}

// reference: OverlappingContourCombiner<MultiAndTrueDistanceSelector>::distance()
msdfcc_mtsdf_distance_t
msdfcc_mtsdf_overlap_contour_combiner_distance(
    const msdfcc_mtsdf_overlap_contour_combiner_t* combiner,
    const msdfcc_shape_t* shape) noexcept {
    assert(combiner);
    msdfcc_mtsdf_distance_t result;
    init_mtsdf_distance(&result);
    if (!combiner->edge_selectors || combiner->contour_count == 0)
        return result;

    const int contour_count = (int)combiner->contour_count;
    const msdfcc_point_t p = combiner->p;

    // shape, inner, outer - work selectors
    msdfcc_mtsdf_edge_selector_t shape_sel, inner_sel, outer_sel;
    mtsdf_edge_selector_reset(&shape_sel, p);
    mtsdf_edge_selector_reset(&inner_sel, p);
    mtsdf_edge_selector_reset(&outer_sel, p);

    for (int i = 0; i < contour_count; ++i) {
        msdfcc_mtsdf_distance_t edge_dist = mtsdf_selector_distance(
            &combiner->edge_selectors[i], p, shape);
        msdfcc_float_t resolved = resolve_mtsdf_distance(&edge_dist);
        mtsdf_selector_merge(&shape_sel, &combiner->edge_selectors[i]);
        if (combiner->windings[i] > 0 && resolved >= 0)
            mtsdf_selector_merge(&inner_sel, &combiner->edge_selectors[i]);
        if (combiner->windings[i] < 0 && resolved <= 0)
            mtsdf_selector_merge(&outer_sel, &combiner->edge_selectors[i]);
    }

    msdfcc_mtsdf_distance_t shape_distance = mtsdf_selector_distance(&shape_sel, p, shape);
    msdfcc_mtsdf_distance_t inner_distance = mtsdf_selector_distance(&inner_sel, p, shape);
    msdfcc_mtsdf_distance_t outer_distance = mtsdf_selector_distance(&outer_sel, p, shape);
    msdfcc_float_t inner_scalar = resolve_mtsdf_distance(&inner_distance);
    msdfcc_float_t outer_scalar = resolve_mtsdf_distance(&outer_distance);

    int winding = 0;
    if (inner_scalar >= 0 && msdfcc_abs(inner_scalar) <= msdfcc_abs(outer_scalar)) {
        result = inner_distance;
        winding = 1;
        for (int i = 0; i < contour_count; ++i) {
            if (combiner->windings[i] > 0) {
                msdfcc_mtsdf_distance_t contour_dist = mtsdf_selector_distance(
                    &combiner->edge_selectors[i], p, shape);
                if (msdfcc_abs(resolve_mtsdf_distance(&contour_dist)) < msdfcc_abs(outer_scalar) &&
                    resolve_mtsdf_distance(&contour_dist) > resolve_mtsdf_distance(&result))
                    result = contour_dist;
            }
        }
    }
    else if (outer_scalar <= 0 && msdfcc_abs(outer_scalar) < msdfcc_abs(inner_scalar)) {
        result = outer_distance;
        winding = -1;
        for (int i = 0; i < contour_count; ++i) {
            if (combiner->windings[i] < 0) {
                msdfcc_mtsdf_distance_t contour_dist = mtsdf_selector_distance(
                    &combiner->edge_selectors[i], p, shape);
                if (msdfcc_abs(resolve_mtsdf_distance(&contour_dist)) < msdfcc_abs(inner_scalar) &&
                    resolve_mtsdf_distance(&contour_dist) < resolve_mtsdf_distance(&result))
                    result = contour_dist;
            }
        }
    }
    else {
        return shape_distance;
    }

    for (int i = 0; i < contour_count; ++i) {
        if (combiner->windings[i] != winding) {
            msdfcc_mtsdf_distance_t contour_dist = mtsdf_selector_distance(
                &combiner->edge_selectors[i], p, shape);
            msdfcc_float_t cr = resolve_mtsdf_distance(&contour_dist);
            msdfcc_float_t dr = resolve_mtsdf_distance(&result);
            if (cr * dr >= 0 && msdfcc_abs(cr) < msdfcc_abs(dr))
                result = contour_dist;
        }
    }
    if (resolve_mtsdf_distance(&result) == resolve_mtsdf_distance(&shape_distance))
        result = shape_distance;
    return result;
}

/* reference: OverlappingContourCombiner::reset(p) -> edgeSelectors[i].reset(p); each MultiDistanceSelector::reset(p) uses delta = (p - this->p).length()*FACTOR and relax(delta). So we always relax(delta) here; combiner->p set in init to (0,0). */
void
msdfcc_mtsdf_overlap_contour_combiner_reset(msdfcc_mtsdf_overlap_contour_combiner_t* combiner, msdfcc_point_t p) noexcept {
    if (!combiner->edge_selectors) return;
    msdfcc_float_t delta = (msdfcc_float_t)MSDFCC_DISTANCE_DELTA_FACTOR * point_length(point_sub(p, combiner->p));
    const uint32_t count = combiner->contour_count;
    for (uint32_t i = 0; i < count; ++i)
        mtsdf_edge_selector_relax(&combiner->edge_selectors[i], p, delta);
    combiner->p = p;
}

// reference: ShapeDistanceFinder.hpp ShapeDistanceFinder::distance(const Point2 &origin)
msdfcc_mtsdf_distance_t
shape_distance_finder_overlap_mtsdf_distance(
    const msdfcc_shape_t* shape,
    msdfcc_mtsdf_overlap_contour_combiner_t* contour_combiner,
    msdfcc_mtsdf_selector_edge_cache_t* edge_cache,
    msdfcc_point_t origin) noexcept {
    msdfcc_mtsdf_distance_t result;
    init_mtsdf_distance(&result);
    assert(shape && contour_combiner);
    if (!edge_cache)
        return result;

    msdfcc_mtsdf_overlap_contour_combiner_reset(contour_combiner, origin);

    msdfcc_mtsdf_selector_edge_cache_t* cache_ptr = edge_cache;

    const uint32_t n_contours = shape->length;
    for (uint32_t ci = 0; ci < n_contours; ++ci) {
        const msdfcc_contour_t* c = shape->contours[ci];
        if (c->length == 0)
            continue;

        const uint32_t m = c->length;
        msdfcc_mtsdf_edge_selector_t* selector = &contour_combiner->edge_selectors[ci];

        // reference: ShapeDistanceFinder.hpp - prevEdge/curEdge/nextEdge pointer walk
        const msdfcc_edge_t* segs = &c->segments[0];
        const msdfcc_edge_t* prev_edge = (m >= 2) ? (segs + (m - 2)) : segs;
        const msdfcc_edge_t* cur_edge = segs + (m - 1);
        const msdfcc_edge_t* end = segs + m;

        for (const msdfcc_edge_t* next_edge = segs; next_edge != end; ++next_edge) {
            msdfcc_mtsdf_selector_add_edge(selector, cache_ptr++, prev_edge, cur_edge, next_edge);
            prev_edge = cur_edge;
            cur_edge = next_edge;
        }
    }

    return msdfcc_mtsdf_overlap_contour_combiner_distance(contour_combiner, shape);
}


// reference: Projection::unproject - coord/scale - translate
static inline msdfcc_point_t projection_unproject(
    const msdfcc_projection_t* proj, msdfcc_float_t px, msdfcc_float_t py) noexcept {
    msdfcc_point_t out;
    out.x = (proj->scale.x != 0) ? (px / proj->scale.x - proj->translate.x) : 0;
    out.y = (proj->scale.y != 0) ? (py / proj->scale.y - proj->translate.y) : 0;
    return out;
}

// reference: DistanceMapping::operator() - scale*(d+translate)
static inline float distance_mapping_apply(
    const msdfcc_distance_mapping_t* m, msdfcc_float_t d) noexcept {
    return (float)(m->scale * (d + m->translate));
}

// reference: msdfgen.cpp generateDistanceField - constructs distanceFinder (combiner + edge_cache) inside
static void
generate_distance_field_mtsdf_overlap(
    const msdfcc_shape_t* shape,
    msdfcc_rgba_t* bitmap,
    msdfcc_size2d_t size,
    const msdfcc_sdf_transformation_t* transformation) noexcept {
    if (!shape || !bitmap || !transformation || size.width == 0 || size.height == 0)
        return;
#ifdef MSDFCC_USE_OPENMP
#pragma omp parallel
#endif
    {
        const int width = size.width;
        const int height = size.height;
        const int row_stride = msdfcc_size2d_row_stride(size);

        msdfcc_mtsdf_overlap_contour_combiner_t combiner = { 0 };
        const uint32_t edge_count = msdfcc_shape_edge_count(shape);
        const size_t cache_size = (size_t)edge_count * sizeof(msdfcc_mtsdf_selector_edge_cache_t);
        msdfcc_mtsdf_selector_edge_cache_t* const edge_cache = (msdfcc_mtsdf_selector_edge_cache_t*)msdfcc_malloc(cache_size);
        if (edge_cache && msdfcc_mtsdf_overlap_contour_combiner_init(&combiner, shape)) {
            memset(edge_cache, 0, cache_size);

            const msdfcc_projection_t* proj = &transformation->projection;
            const msdfcc_distance_mapping_t* dm = &transformation->distance_mapping;

            int x_direction = 1;
            int y;
#ifdef MSDFCC_USE_OPENMP
#pragma omp for
#endif
            for (y = 0; y < height; ++y) {
                int x_start = (x_direction < 0) ? width - 1 : 0;
                for (int col = 0; col < width; ++col) {
                    int x = x_start + col * x_direction;
                    msdfcc_point_t p = projection_unproject(proj, (msdfcc_float_t)x + MSDFCC_LIT(0.5), (msdfcc_float_t)y + MSDFCC_LIT(0.5));

                    msdfcc_mtsdf_distance_t dist = shape_distance_finder_overlap_mtsdf_distance(
                        shape, &combiner, edge_cache, p);

                    msdfcc_rgba_t* pixel = bitmap + y * row_stride + x;
                    pixel->r = distance_mapping_apply(dm, dist.r);
                    pixel->g = distance_mapping_apply(dm, dist.g);
                    pixel->b = distance_mapping_apply(dm, dist.b);
                    pixel->a = distance_mapping_apply(dm, dist.a);
                }
                x_direction = -x_direction;
            }

        }


        msdfcc_mtsdf_overlap_contour_combiner_uninit(&combiner);
        msdfcc_free(edge_cache);
    }

}

// reference: rasterization.cpp distanceSignCorrection / multiDistanceSignCorrection
void
msdfcc_distance_sign_correction(
    msdfcc_rgba_t* bitmap,
    msdfcc_size2d_t size,
    const msdfcc_shape_t* shape,
    const msdfcc_projection_t* projection,
    float sdf_zero_value,
    msdfcc_fill_rule_t fill_rule) noexcept {
    if (!bitmap || !shape || !projection || size.width == 0 || size.height == 0)
        return;
    const int w = size.width;
    const int h = size.height;
    const int row_stride = msdfcc_size2d_row_stride(size);
    const float double_sdf_zero = sdf_zero_value + sdf_zero_value;

    uint32_t max_intersections = 3 * msdfcc_shape_edge_count(shape);
    if (max_intersections == 0) return;
    const size_t match_size = (size_t)w * h;
    const size_t scanline_size = (size_t)max_intersections * sizeof(scanline_intersection_t);
    void* const alloc_base = msdfcc_malloc(scanline_size + match_size);
    if (!alloc_base) return;
    scanline_intersection_t* scanline_buf = (scanline_intersection_t*)alloc_base;
    char* match_map = (char*)((char*)alloc_base + scanline_size);
    memset(match_map, 0, match_size);

    int ambiguous = 0;
    for (int y = 0; y < h; ++y) {
        msdfcc_float_t shape_y = (projection->scale.y != 0) ? ((msdfcc_float_t)y + MSDFCC_LIT(0.5)) / projection->scale.y - projection->translate.y : 0;
        uint32_t n_isec = shape_scanline_at_y(shape, shape_y, scanline_buf, max_intersections);

        for (int x = 0; x < w; ++x) {
            msdfcc_float_t shape_x = (projection->scale.x != 0) ? ((msdfcc_float_t)x + MSDFCC_LIT(0.5)) / projection->scale.x - projection->translate.x : 0;
            int fill = interpret_fill_rule(scanline_sum_at_x(scanline_buf, n_isec, shape_x), fill_rule);

            msdfcc_rgba_t* msd = bitmap + y * row_stride + x;
            float sd = median3f(msd->r, msd->g, msd->b);

            if (sd == sdf_zero_value) {
                ambiguous = 1;
                // match stays 0 for ambiguous pass
            }
            else if ((sd > sdf_zero_value) != fill) {
                msd->r = double_sdf_zero - msd->r;
                msd->g = double_sdf_zero - msd->g;
                msd->b = double_sdf_zero - msd->b;
                match_map[y * w + x] = (char)-1;
            }
            else {
                match_map[y * w + x] = 1;
            }

            if ((msd->a > sdf_zero_value) != fill)
                msd->a = double_sdf_zero - msd->a;
        }
    }

    // Avoid artifacts when whole shape is inverted (reference: multiDistanceSignCorrection ambiguous pass)
    if (ambiguous) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (match_map[y * w + x] != 0) continue;
                int neighbor_match = 0;
                if (x > 0) neighbor_match += (int)match_map[y * w + (x - 1)];
                if (x < w - 1) neighbor_match += (int)match_map[y * w + (x + 1)];
                if (y > 0) neighbor_match += (int)match_map[(size_t)(y - 1) * w + x];
                if (y < h - 1) neighbor_match += (int)match_map[(size_t)(y + 1) * w + x];
                if (neighbor_match < 0) {
                    msdfcc_rgba_t* msd = bitmap + y * row_stride + x;
                    msd->r = double_sdf_zero - msd->r;
                    msd->g = double_sdf_zero - msd->g;
                    msd->b = double_sdf_zero - msd->b;
                }
            }
        }
    }

    msdfcc_free(alloc_base);
}

// ----------------------------------------------------------------------------
//         msdfErrorCorrection (ref: msdf-error-correction.cpp)
// ----------------------------------------------------------------------------

#define MSDFCC_EC_STENCIL_ERROR    1
#define MSDFCC_EC_STENCIL_PROTECTED 2
#define MSDFCC_EC_ARTIFACT_T_EPSILON 0.01
#define MSDFCC_EC_PROTECTION_RADIUS_TOLERANCE 1.001

// projection_project: scale*(coord+translate)
static inline msdfcc_point_t projection_project(
    const msdfcc_projection_t* proj, msdfcc_float_t sx, msdfcc_float_t sy) noexcept {
    msdfcc_point_t out;
    out.x = proj->scale.x * (sx + proj->translate.x);
    out.y = proj->scale.y * (sy + proj->translate.y);
    return out;
}

// unprojectVector: (vx/scale.x, vy/scale.y)
static inline msdfcc_point_t projection_unproject_vector(
    const msdfcc_projection_t* proj, msdfcc_float_t vx, msdfcc_float_t vy) noexcept {
    msdfcc_point_t out;
    out.x = (proj->scale.x != 0) ? (vx / proj->scale.x) : 0;
    out.y = (proj->scale.y != 0) ? (vy / proj->scale.y) : 0;
    return out;
}

// DistanceMapping(Delta(1)) = scale * 1
static inline msdfcc_float_t distance_mapping_delta(
    const msdfcc_distance_mapping_t* dm, msdfcc_float_t delta) noexcept {
    return dm->scale * delta;
}

// reference: MSDFErrorCorrection::protectCorners
static void ec_protect_corners(
    const msdfcc_shape_t* shape, unsigned char* stencil,
    msdfcc_size2d_t size,
    const msdfcc_sdf_transformation_t* xform) noexcept {
    if (!shape || !stencil) return;
    const int width = size.width;
    const int height = size.height;
    const int row_stride = msdfcc_size2d_row_stride(size);
    const msdfcc_projection_t* proj = &xform->projection;
    const uint32_t n_contours = shape->length;
    for (uint32_t ci = 0; ci < n_contours; ++ci) {
        const msdfcc_contour_t* c = shape->contours[ci];
        if (c->length == 0) continue;

        uint32_t m = c->length;
        int prev_color = (int)c->segments[m - 1].color;
        for (uint32_t ei = 0; ei < m; ++ei) {
            int cur_color = (int)c->segments[ei].color;
            int common = prev_color & cur_color;
            if (common != 0 && (common & (common - 1)) == 0) {
                msdfcc_point_t pt = edge_point(&c->segments[ei], MSDFCC_LIT(0));
                msdfcc_point_t p_proj = projection_project(proj, pt.x, pt.y);
                int l = (int)msdfcc_floor(p_proj.x - MSDFCC_LIT(0.5));
                int b = (int)msdfcc_floor(p_proj.y - MSDFCC_LIT(0.5));
                int r = l + 1, t = b + 1;
                if (l < width && b < height && r >= 0 && t >= 0) {
                    if (l >= 0 && b >= 0) stencil[(size_t)b * row_stride + l] |= MSDFCC_EC_STENCIL_PROTECTED;
                    if (r < width && b >= 0) stencil[(size_t)b * row_stride + r] |= MSDFCC_EC_STENCIL_PROTECTED;
                    if (l >= 0 && t < height) stencil[(size_t)t * row_stride + l] |= MSDFCC_EC_STENCIL_PROTECTED;
                    if (r < width && t < height) stencil[(size_t)t * row_stride + r] |= MSDFCC_EC_STENCIL_PROTECTED;
                }
            }
            prev_color = cur_color;
        }
    }
}

// reference: edgeBetweenTexelsChannel, edgeBetweenTexels, protectExtremeChannels
static bool edge_between_texels_channel(const float* a, const float* b, int ch) noexcept {
    const msdfcc_float_t t = (msdfcc_float_t)(a[ch] - 0.5f) / (msdfcc_float_t)(a[ch] - b[ch]);
    if (t > 0 && t < 1) {
        const float c0 = mixf(a[0], b[0], t);
        const float c1 = mixf(a[1], b[1], t);
        const float c2 = mixf(a[2], b[2], t);
        const float cm = median3f(c0, c1, c2);
        return cm == (ch == 0 ? c0 : ch == 1 ? c1 : c2);
    }
    return false;
}

static int edge_between_texels(const float* a, const float* b) noexcept {
    return ((int)edge_between_texels_channel(a, b, 0) * 1) +
        ((int)edge_between_texels_channel(a, b, 1) * 2) +
        ((int)edge_between_texels_channel(a, b, 2) * 4);
}

static void protect_extreme_channels(unsigned char* stencil, const float* msd, float m, int mask) noexcept {
    if ((mask & 1 && msd[0] != m) || (mask & 2 && msd[1] != m) || (mask & 4 && msd[2] != m))
        *stencil |= MSDFCC_EC_STENCIL_PROTECTED;
}

// reference: MSDFErrorCorrection::protectEdges
static void ec_protect_edges(
    const msdfcc_rgba_t* bitmap, unsigned char* stencil,
    msdfcc_size2d_t size,
    const msdfcc_sdf_transformation_t* xform) noexcept {
    const int width = size.width;
    const int height = size.height;
    const int row_stride = msdfcc_size2d_row_stride(size);
    const msdfcc_projection_t* proj = &xform->projection;
    const msdfcc_distance_mapping_t* dm = &xform->distance_mapping;
    msdfcc_float_t delta1 = distance_mapping_delta(dm, MSDFCC_LIT(1.0));

    msdfcc_point_t hv = projection_unproject_vector(proj, delta1, MSDFCC_LIT(0));
    float radius = (float)(MSDFCC_EC_PROTECTION_RADIUS_TOLERANCE * msdfcc_sqrt(hv.x * hv.x + hv.y * hv.y));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x + 1 < width; ++x) {
            const float* left = (const float*)(bitmap + y * row_stride + x);
            const float* right = (const float*)(bitmap + y * row_stride + x + 1);
            float lm = median3f(left[0], left[1], left[2]);
            float rm = median3f(right[0], right[1], right[2]);
            if (fabsf(lm - 0.5f) + fabsf(rm - 0.5f) < radius) {
                int mask = edge_between_texels(left, right);
                protect_extreme_channels(&stencil[y * row_stride + x], left, lm, mask);
                protect_extreme_channels(&stencil[y * row_stride + x + 1], right, rm, mask);
            }
        }
    }
    hv = projection_unproject_vector(proj, MSDFCC_LIT(0), delta1);
    radius = (float)(MSDFCC_EC_PROTECTION_RADIUS_TOLERANCE * msdfcc_sqrt(hv.x * hv.x + hv.y * hv.y));
    for (int y = 0; y + 1 < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float* bot = (const float*)(bitmap + y * row_stride + x);
            const float* top = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x);
            float bm = median3f(bot[0], bot[1], bot[2]);
            float tm = median3f(top[0], top[1], top[2]);
            if (fabsf(bm - 0.5f) + fabsf(tm - 0.5f) < radius) {
                int mask = edge_between_texels(bot, top);
                protect_extreme_channels(&stencil[y * row_stride + x], bot, bm, mask);
                protect_extreme_channels(&stencil[(size_t)(y + 1) * row_stride + x], top, tm, mask);
            }
        }
    }
    hv = projection_unproject_vector(proj, delta1, delta1);
    radius = (float)(MSDFCC_EC_PROTECTION_RADIUS_TOLERANCE * msdfcc_sqrt(hv.x * hv.x + hv.y * hv.y));
    for (int y = 0; y + 1 < height; ++y) {
        for (int x = 0; x + 1 < width; ++x) {
            const float* lb = (const float*)(bitmap + y * row_stride + x);
            const float* rb = (const float*)(bitmap + y * row_stride + x + 1);
            const float* lt = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x);
            const float* rt = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x + 1);
            float mlb = median3f(lb[0], lb[1], lb[2]);
            float mrt = median3f(rt[0], rt[1], rt[2]);
            if (fabsf(mlb - 0.5f) + fabsf(mrt - 0.5f) < radius) {
                int mask = edge_between_texels(lb, rt);
                protect_extreme_channels(&stencil[y * row_stride + x], lb, mlb, mask);
                protect_extreme_channels(&stencil[(size_t)(y + 1) * row_stride + x + 1], rt, mrt, mask);
            }
            float mrb = median3f(rb[0], rb[1], rb[2]);
            float mlt = median3f(lt[0], lt[1], lt[2]);
            if (fabsf(mrb - 0.5f) + fabsf(mlt - 0.5f) < radius) {
                int mask = edge_between_texels(rb, lt);
                protect_extreme_channels(&stencil[y * row_stride + x + 1], rb, mrb, mask);
                protect_extreme_channels(&stencil[(size_t)(y + 1) * row_stride + x], lt, mlt, mask);
            }
        }
    }
}

static void ec_protect_all(unsigned char* stencil, msdfcc_size2d_t size) noexcept {
    const int width = size.width;
    const int height = size.height;
    const int row_stride = msdfcc_size2d_row_stride(size);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            stencil[y * row_stride + x] |= MSDFCC_EC_STENCIL_PROTECTED;
}

// reference: BaseArtifactClassifier::rangeTest
static int ec_range_test(msdfcc_float_t span, int protected_flag, msdfcc_float_t at, msdfcc_float_t bt, msdfcc_float_t xt, float am, float bm, float xm) noexcept {
    if ((am > 0.5f && bm > 0.5f && xm <= 0.5f) || (am < 0.5f && bm < 0.5f && xm >= 0.5f) ||
        (!protected_flag && median3f(am, bm, xm) != xm)) {
        const msdfcc_float_t ax_span = (xt - at) * span;
        const msdfcc_float_t bx_span = (bt - xt) * span;
        if (!(xm >= am - (float)ax_span && xm <= am + (float)ax_span && xm >= bm - (float)bx_span && xm <= bm + (float)bx_span))
            return 3; // CANDIDATE|ARTIFACT
        return 1; // CANDIDATE
    }
    return 0;
}

static float interpolated_median_linear(const float* a, const float* b, msdfcc_float_t t) noexcept {
    return median3f(mixf(a[0], b[0], t), mixf(a[1], b[1], t), mixf(a[2], b[2], t));
}

// reference: hasLinearArtifactInner
static bool has_linear_artifact_inner(
    msdfcc_float_t span, int protected_flag, float am, float bm, const float* a, const float* b, float dA, float dB) noexcept {
    const msdfcc_float_t t = (msdfcc_float_t)dA / (msdfcc_float_t)(dA - dB);
    if (t > (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON && t < MSDFCC_LIT(1.0) - (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON) {
        const float xm = interpolated_median_linear(a, b, t);
        const int flags = ec_range_test(span, protected_flag, 0, 1, t, am, bm, xm);
        // BaseArtifactClassifier::evaluate only checks flags&2 (ARTIFACT flag)
        return (flags & 2) != 0;
    }
    return false;
}

// reference: hasLinearArtifact
static bool has_linear_artifact(
    msdfcc_float_t span, int protected_flag, float am, const float* a, const float* b) noexcept {
    const float bm = median3f(b[0], b[1], b[2]);
    if (fabsf(am - 0.5f) >= fabsf(bm - 0.5f)) {
        return has_linear_artifact_inner(span, protected_flag, am, bm, a, b, a[1] - a[0], b[1] - b[0]) ||
            has_linear_artifact_inner(span, protected_flag, am, bm, a, b, a[2] - a[1], b[2] - b[1]) ||
            has_linear_artifact_inner(span, protected_flag, am, bm, a, b, a[0] - a[2], b[0] - b[2]);
    }
    return false;
}

static float interpolated_median_bilinear(const float* a, const float* l, const float* q, msdfcc_float_t t) noexcept {
    return median3f(
        (float)(t * (t * q[0] + l[0]) + a[0]),
        (float)(t * (t * q[1] + l[1]) + a[1]),
        (float)(t * (t * q[2] + l[2]) + a[2]));
}

// reference: hasDiagonalArtifactInner (simplified - no shape check in base path)
static bool has_diagonal_artifact_inner(
    msdfcc_float_t span, int protected_flag, float am, float dm, const float* a, const float* l, const float* q,
    float dA, float dBC, float dD, msdfcc_float_t tEx0, msdfcc_float_t tEx1) noexcept {
    const msdfcc_float_t coeffs[3] = { (msdfcc_float_t)(dD - dBC + dA), (msdfcc_float_t)(dBC - dA - dA), (msdfcc_float_t)dA };
    msdfcc_float_t t[2];
    const int n = solve_quadratic(t, coeffs[0], coeffs[1], coeffs[2]);
    for (int i = 0; i < n; ++i) {
        if (t[i] > (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON && t[i] < MSDFCC_LIT(1.0) - (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON) {
            const float xm = interpolated_median_bilinear(a, l, q, t[i]);
            int flags = ec_range_test(span, protected_flag, 0, 1, t[i], am, dm, xm);
            if (tEx0 > 0 && tEx0 < 1) {
                const float em = interpolated_median_bilinear(a, l, q, tEx0);
                msdfcc_float_t te0 = 0, te1 = MSDFCC_LIT(1.0);
                if (tEx0 > t[i]) te1 = tEx0; else te0 = tEx0;
                flags |= ec_range_test(span, protected_flag, te0, te1, t[i],
                    (tEx0 > t[i]) ? am : em, (tEx0 > t[i]) ? em : dm, xm);
            }
            if (tEx1 > 0 && tEx1 < 1) {
                const float em = interpolated_median_bilinear(a, l, q, tEx1);
                msdfcc_float_t te0 = 0, te1 = MSDFCC_LIT(1.0);
                if (tEx1 > t[i]) te1 = tEx1; else te0 = tEx1;
                flags |= ec_range_test(span, protected_flag, te0, te1, t[i],
                    (tEx1 > t[i]) ? am : em, (tEx1 > t[i]) ? em : dm, xm);
            }
            if (flags & 2) return true;
        }
    }
    return false;
}

// reference: hasDiagonalArtifact
static bool has_diagonal_artifact(
    msdfcc_float_t span, int protected_flag, float am, const float* a, const float* b, const float* c, const float* d) noexcept {
    const float dm = median3f(d[0], d[1], d[2]);
    if (fabsf(am - 0.5f) >= fabsf(dm - 0.5f)) {
        const float abc[3] = { a[0] - b[0] - c[0], a[1] - b[1] - c[1], a[2] - b[2] - c[2] };
        const float l[3] = { -a[0] - abc[0], -a[1] - abc[1], -a[2] - abc[2] };
        const float q[3] = { d[0] + abc[0], d[1] + abc[1], d[2] + abc[2] };
        const msdfcc_float_t tEx0 = (q[0] != 0) ? MSDFCC_LIT(-0.5) * (msdfcc_float_t)l[0] / (msdfcc_float_t)q[0] : 0;
        const msdfcc_float_t tEx1 = (q[1] != 0) ? MSDFCC_LIT(-0.5) * (msdfcc_float_t)l[1] / (msdfcc_float_t)q[1] : 0;
        const msdfcc_float_t tEx2 = (q[2] != 0) ? MSDFCC_LIT(-0.5) * (msdfcc_float_t)l[2] / (msdfcc_float_t)q[2] : 0;
        return has_diagonal_artifact_inner(span, protected_flag, am, dm, a, l, q, a[1] - a[0], b[1] - b[0] + c[1] - c[0], d[1] - d[0], tEx0, tEx1) ||
            has_diagonal_artifact_inner(span, protected_flag, am, dm, a, l, q, a[2] - a[1], b[2] - b[1] + c[2] - c[1], d[2] - d[1], tEx1, tEx2) ||
            has_diagonal_artifact_inner(span, protected_flag, am, dm, a, l, q, a[0] - a[2], b[0] - b[2] + c[0] - c[2], d[0] - d[2], tEx2, tEx0);
    }
    return false;
}


// reference: bitmap-interpolation.hpp interpolate - bilinear sample at pixel position (px, py in [0,width],[0,height])
static void interpolate_at(const msdfcc_rgba_t* bitmap, msdfcc_size2d_t size,
    msdfcc_float_t px, msdfcc_float_t py, float out_rgb[3]) noexcept {
    const int width = size.width;
    const int height = size.height;
    const int row_stride = msdfcc_size2d_row_stride(size);
    if (px < 0) px = 0; else if (px > (msdfcc_float_t)width) px = (msdfcc_float_t)width;
    if (py < 0) py = 0; else if (py > (msdfcc_float_t)height) py = (msdfcc_float_t)height;
    px -= MSDFCC_LIT(0.5);
    py -= MSDFCC_LIT(0.5);
    int l = (int)msdfcc_floor(px), b = (int)msdfcc_floor(py);
    int r = l + 1, t = b + 1;
    msdfcc_float_t lr = px - (msdfcc_float_t)l, bt = py - (msdfcc_float_t)b;
    if (l < 0) l = 0; if (l > width - 1) l = width - 1;
    if (r < 0) r = 0; if (r > width - 1) r = width - 1;
    if (b < 0) b = 0; if (b > height - 1) b = height - 1;
    if (t < 0) t = 0; if (t > height - 1) t = height - 1;
    const float* lb = (const float*)(bitmap + (size_t)b * row_stride + l);
    const float* rb = (const float*)(bitmap + (size_t)b * row_stride + r);
    const float* lt = (const float*)(bitmap + (size_t)t * row_stride + l);
    const float* rt = (const float*)(bitmap + (size_t)t * row_stride + r);
    for (int i = 0; i < 3; ++i)
        out_rgb[i] = mixf(mixf(lb[i], rb[i], lr), mixf(lt[i], rt[i], lr), bt);
}

// Reference PSD at shape point: median(r,g,b) of MTSDF distance then distance_mapping.
static float shape_ref_psd(const msdfcc_shape_t* shape,
    msdfcc_mtsdf_overlap_contour_combiner_t* combiner, msdfcc_mtsdf_selector_edge_cache_t* edge_cache,
    msdfcc_point_t shape_point, const msdfcc_distance_mapping_t* dm) noexcept {
    msdfcc_mtsdf_distance_t d = shape_distance_finder_overlap_mtsdf_distance(shape, combiner, edge_cache, shape_point);
    msdfcc_float_t med = (msdfcc_float_t)median3f((float)d.r, (float)d.g, (float)d.b);
    return distance_mapping_apply(dm, med);
}

// forward decl for use in ec_diagonal_artifact_inner_with_shape
static bool shape_distance_evaluate(
    const msdfcc_rgba_t* bitmap, msdfcc_size2d_t size,
    const msdfcc_shape_t* shape, msdfcc_mtsdf_overlap_contour_combiner_t* combiner,
    msdfcc_mtsdf_selector_edge_cache_t* edge_cache,
    const msdfcc_sdf_transformation_t* xform, msdfcc_float_t min_improve_ratio,
    msdfcc_float_t sdf_x, msdfcc_float_t sdf_y, msdfcc_point_t shape_coord, msdfcc_float_t dir_x, msdfcc_float_t dir_y, msdfcc_float_t t,
    const float* msd, float a_psd) noexcept;

// reference: hasDiagonalArtifactInner(artifactClassifier, am, dm, a, l, q, dA, dBC, dD, tEx0, tEx1) + ShapeDistanceChecker::evaluate
static inline bool
ec_diagonal_artifact_inner_with_shape(
    const msdfcc_rgba_t* bitmap, msdfcc_size2d_t size,
    const msdfcc_shape_t* shape, msdfcc_mtsdf_overlap_contour_combiner_t* combiner,
    msdfcc_mtsdf_selector_edge_cache_t* edge_cache, const msdfcc_sdf_transformation_t* xform,
    msdfcc_float_t min_improve_ratio, msdfcc_float_t d_span, int protected_flag,
    float am, float dm, const float* a, const float* l, const float* q,
    float dA, float dBC, float dD, msdfcc_float_t tEx0, msdfcc_float_t tEx1,
    msdfcc_float_t sdf_x, msdfcc_float_t sdf_y, msdfcc_point_t shape_coord, int dir_x, int dir_y) noexcept {
    const msdfcc_float_t coeff_a = (msdfcc_float_t)(dD - dBC + dA);
    const msdfcc_float_t coeff_b = (msdfcc_float_t)(dBC - dA - dA);
    const msdfcc_float_t coeff_c = (msdfcc_float_t)dA;
    msdfcc_float_t t[2];
    const int solutions = solve_quadratic(t, coeff_a, coeff_b, coeff_c);
    if (solutions < 0) return false;
    for (int i = 0; i < solutions; ++i) {
        if (t[i] <= (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON || t[i] >= MSDFCC_LIT(1.0) - (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON)
            continue;
        const float xm = interpolated_median_bilinear(a, l, q, t[i]);
        int range_flags = ec_range_test(d_span, protected_flag, 0, 1, t[i], am, dm, xm);
        if (tEx0 > 0 && tEx0 < 1) {
            const float em = interpolated_median_bilinear(a, l, q, tEx0);
            msdfcc_float_t t_end0 = 0, t_end1 = MSDFCC_LIT(1.0);
            if (tEx0 > t[i]) t_end1 = tEx0; else t_end0 = tEx0;
            range_flags |= ec_range_test(d_span, protected_flag, t_end0, t_end1, t[i],
                (tEx0 > t[i]) ? am : em, (tEx0 > t[i]) ? em : dm, xm);
        }
        if (tEx1 > 0 && tEx1 < 1) {
            const float em = interpolated_median_bilinear(a, l, q, tEx1);
            msdfcc_float_t t_end0 = 0, t_end1 = MSDFCC_LIT(1.0);
            if (tEx1 > t[i]) t_end1 = tEx1; else t_end0 = tEx1;
            range_flags |= ec_range_test(d_span, protected_flag, t_end0, t_end1, t[i],
                (tEx1 > t[i]) ? am : em, (tEx1 > t[i]) ? em : dm, xm);
        }
        if (range_flags & 2) return true;
        if ((range_flags & 1) && shape_distance_evaluate(bitmap, size, shape, combiner, edge_cache, xform, min_improve_ratio, sdf_x, sdf_y, shape_coord, (msdfcc_float_t)dir_x, (msdfcc_float_t)dir_y, t[i], a, am)) return true;
    }
    return false;
}

// reference: hasDiagonalArtifact(artifactClassifier, am, a, b, c, d) - calls inner for each channel pair
static bool
ec_diagonal_artifact_with_shape(
    const msdfcc_rgba_t* bitmap,
    msdfcc_size2d_t size,
    const msdfcc_shape_t* shape, msdfcc_mtsdf_overlap_contour_combiner_t* combiner,
    msdfcc_mtsdf_selector_edge_cache_t* edge_cache,
    const msdfcc_sdf_transformation_t* xform, msdfcc_float_t min_deviation_ratio, msdfcc_float_t min_improve_ratio,
    const float* a, const float* b, const float* c, const float* d, float am, msdfcc_float_t d_span, int protected_flag,
    msdfcc_float_t sdf_x, msdfcc_float_t sdf_y, msdfcc_point_t shape_coord, int dir_x, int dir_y) noexcept {
    const float dm = median3f(d[0], d[1], d[2]);
    if (fabsf(am - 0.5f) < fabsf(dm - 0.5f)) return false;
    const float abc[3] = { a[0] - b[0] - c[0], a[1] - b[1] - c[1], a[2] - b[2] - c[2] };
    const float l[3] = { -a[0] - abc[0], -a[1] - abc[1], -a[2] - abc[2] };
    const float q[3] = { d[0] + abc[0], d[1] + abc[1], d[2] + abc[2] };
    msdfcc_float_t tEx[3];
    tEx[0] = (q[0] != 0) ? MSDFCC_LIT(-0.5) * (msdfcc_float_t)l[0] / (msdfcc_float_t)q[0] : 0;
    tEx[1] = (q[1] != 0) ? MSDFCC_LIT(-0.5) * (msdfcc_float_t)l[1] / (msdfcc_float_t)q[1] : 0;
    tEx[2] = (q[2] != 0) ? MSDFCC_LIT(-0.5) * (msdfcc_float_t)l[2] / (msdfcc_float_t)q[2] : 0;
    return ec_diagonal_artifact_inner_with_shape(bitmap, size, shape, combiner, edge_cache, xform, min_improve_ratio, d_span, protected_flag, am, dm, a, l, q, a[1] - a[0], b[1] - b[0] + c[1] - c[0], d[1] - d[0], tEx[0], tEx[1], sdf_x, sdf_y, shape_coord, dir_x, dir_y) ||
        ec_diagonal_artifact_inner_with_shape(bitmap, size, shape, combiner, edge_cache, xform, min_improve_ratio, d_span, protected_flag, am, dm, a, l, q, a[2] - a[1], b[2] - b[1] + c[2] - c[1], d[2] - d[1], tEx[1], tEx[2], sdf_x, sdf_y, shape_coord, dir_x, dir_y) ||
        ec_diagonal_artifact_inner_with_shape(bitmap, size, shape, combiner, edge_cache, xform, min_improve_ratio, d_span, protected_flag, am, dm, a, l, q, a[0] - a[2], b[0] - b[2] + c[0] - c[2], d[0] - d[2], tEx[2], tEx[0], sdf_x, sdf_y, shape_coord, dir_x, dir_y);
}

// reference: MSDFErrorCorrection::findErrors<OverlappingContourCombiner, N>(sdf, shape) - ShapeDistanceChecker evaluate
static bool shape_distance_evaluate(
    const msdfcc_rgba_t* bitmap, msdfcc_size2d_t size,
    const msdfcc_shape_t* shape, msdfcc_mtsdf_overlap_contour_combiner_t* combiner,
    msdfcc_mtsdf_selector_edge_cache_t* edge_cache,
    const msdfcc_sdf_transformation_t* xform, msdfcc_float_t min_improve_ratio,
    msdfcc_float_t sdf_x, msdfcc_float_t sdf_y, msdfcc_point_t shape_coord, msdfcc_float_t dir_x, msdfcc_float_t dir_y, msdfcc_float_t t,
    const float* msd, float a_psd) noexcept {
    const msdfcc_projection_t* proj = &xform->projection;
    const msdfcc_distance_mapping_t* dm = &xform->distance_mapping;
    const msdfcc_point_t texel_size = projection_unproject_vector(proj, MSDFCC_LIT(1), MSDFCC_LIT(0));
    const msdfcc_point_t texel_size_y = projection_unproject_vector(proj, MSDFCC_LIT(0), MSDFCC_LIT(1));
    const msdfcc_float_t t_dx = t * dir_x;
    const msdfcc_float_t t_dy = t * dir_y;

    float old_msd[3];
    interpolate_at(bitmap, size, sdf_x + t_dx, sdf_y + t_dy, old_msd);
    const float old_psd = median3f(old_msd[0], old_msd[1], old_msd[2]);

    const msdfcc_float_t a_weight = (MSDFCC_LIT(1.0) - msdfcc_abs(t_dx)) * (MSDFCC_LIT(1.0) - msdfcc_abs(t_dy));
    float new_msd[3];
    new_msd[0] = (float)(old_msd[0] + a_weight * (a_psd - msd[0]));
    new_msd[1] = (float)(old_msd[1] + a_weight * (a_psd - msd[1]));
    new_msd[2] = (float)(old_msd[2] + a_weight * (a_psd - msd[2]));
    const float new_psd = median3f(new_msd[0], new_msd[1], new_msd[2]);

    msdfcc_point_t ref_point;
    ref_point.x = shape_coord.x + t_dx * texel_size.x;
    ref_point.y = shape_coord.y + t_dy * texel_size_y.y;
    const float ref_psd = shape_ref_psd(shape, combiner, edge_cache, ref_point, dm);

    return (msdfcc_float_t)min_improve_ratio * (msdfcc_float_t)fabsf(new_psd - ref_psd) < (msdfcc_float_t)fabsf(old_psd - ref_psd);
}

// reference: MSDFErrorCorrection::findErrors<OverlappingContourCombiner, N>(sdf, shape)
static void ec_find_errors_with_shape_overlap(
    const msdfcc_rgba_t* bitmap, unsigned char* stencil,
    msdfcc_size2d_t size,
    const msdfcc_shape_t* shape, const msdfcc_sdf_transformation_t* xform,
    msdfcc_float_t min_deviation_ratio, msdfcc_float_t min_improve_ratio) noexcept {
    if (!shape) return;
    const int width = size.width;
    const int height = size.height;
    const int row_stride = msdfcc_size2d_row_stride(size);
    const msdfcc_projection_t* proj = &xform->projection;
    const msdfcc_distance_mapping_t* dm = &xform->distance_mapping;
    msdfcc_float_t delta1 = distance_mapping_delta(dm, MSDFCC_LIT(1.0));

    msdfcc_point_t h = projection_unproject_vector(proj, delta1, MSDFCC_LIT(0));
    msdfcc_point_t v = projection_unproject_vector(proj, MSDFCC_LIT(0), delta1);
    msdfcc_point_t d = projection_unproject_vector(proj, delta1, delta1);
    msdfcc_float_t h_span = min_deviation_ratio * msdfcc_sqrt(h.x * h.x + h.y * h.y);
    msdfcc_float_t v_span = min_deviation_ratio * msdfcc_sqrt(v.x * v.x + v.y * v.y);
    msdfcc_float_t d_span = min_deviation_ratio * msdfcc_sqrt(d.x * d.x + d.y * d.y);

    msdfcc_mtsdf_overlap_contour_combiner_t combiner;
    if (!msdfcc_mtsdf_overlap_contour_combiner_init(&combiner, shape))
        return;
    uint32_t edge_count = msdfcc_shape_edge_count(shape);
    size_t cache_size = (size_t)edge_count * sizeof(msdfcc_mtsdf_selector_edge_cache_t);
    msdfcc_mtsdf_selector_edge_cache_t* edge_cache = (msdfcc_mtsdf_selector_edge_cache_t*)msdfcc_malloc(cache_size);
    if (!edge_cache) {
        msdfcc_mtsdf_overlap_contour_combiner_uninit(&combiner);
        return;
    }
    memset(edge_cache, 0, cache_size);

    int x_direction = 1;
    for (int y = 0; y < height; ++y) {
        int x = (x_direction < 0) ? width - 1 : 0;
        for (int col = 0; col < width; ++col, x += x_direction) {
            if (stencil[y * row_stride + x] & MSDFCC_EC_STENCIL_ERROR)
                continue;
            const float* c = (const float*)(bitmap + y * row_stride + x);
            float cm = median3f(c[0], c[1], c[2]);
            int protected_flag = (stencil[y * row_stride + x] & MSDFCC_EC_STENCIL_PROTECTED) != 0;
            msdfcc_float_t sdf_x = (msdfcc_float_t)x + MSDFCC_LIT(0.5), sdf_y = (msdfcc_float_t)y + MSDFCC_LIT(0.5);
            msdfcc_point_t shape_coord = projection_unproject(proj, sdf_x, sdf_y);
            float a_psd = cm;

            int err = 0;
            if (x > 0) {
                const float* l = (const float*)(bitmap + y * row_stride + x - 1);
                float lm = median3f(l[0], l[1], l[2]);
                if (fabsf(cm - 0.5f) >= fabsf(lm - 0.5f)) {
                    msdfcc_float_t t_val; float dA, dB;
                    dA = c[1] - c[0]; dB = l[1] - l[0];
                    t_val = (msdfcc_float_t)dA / (msdfcc_float_t)(dA - dB);
                    if (t_val > (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON && t_val < MSDFCC_LIT(1.0) - (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON) {
                        float xm = interpolated_median_linear(c, l, t_val);
                        int flags = ec_range_test(h_span, protected_flag, 0, 1, t_val, cm, lm, xm);
                        if (flags & 2) err = 1;
                        else if (flags & 1 && shape_distance_evaluate(bitmap, size, shape, &combiner, edge_cache, xform, min_improve_ratio, sdf_x, sdf_y, shape_coord, -1, 0, t_val, c, a_psd)) err = 1;
                    }
                    if (!err) { dA = c[2] - c[1]; dB = l[2] - l[1]; t_val = (msdfcc_float_t)dA / (msdfcc_float_t)(dA - dB); if (t_val > (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON && t_val < MSDFCC_LIT(1.0) - (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON) { float xm = interpolated_median_linear(c, l, t_val); int flags = ec_range_test(h_span, protected_flag, 0, 1, t_val, cm, lm, xm); if (flags & 2) err = 1; else if (flags & 1 && shape_distance_evaluate(bitmap, size, shape, &combiner, edge_cache, xform, min_improve_ratio, sdf_x, sdf_y, shape_coord, -1, 0, t_val, c, a_psd)) err = 1; } }
                    if (!err) { dA = c[0] - c[2]; dB = l[0] - l[2]; t_val = (msdfcc_float_t)dA / (msdfcc_float_t)(dA - dB); if (t_val > (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON && t_val < MSDFCC_LIT(1.0) - (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON) { float xm = interpolated_median_linear(c, l, t_val); int flags = ec_range_test(h_span, protected_flag, 0, 1, t_val, cm, lm, xm); if (flags & 2) err = 1; else if (flags & 1 && shape_distance_evaluate(bitmap, size, shape, &combiner, edge_cache, xform, min_improve_ratio, sdf_x, sdf_y, shape_coord, -1, 0, t_val, c, a_psd)) err = 1; } }
                }
            }
            if (!err && y > 0) {
                const float* b = (const float*)(bitmap + (size_t)(y - 1) * row_stride + x);
                float bm = median3f(b[0], b[1], b[2]);
                if (fabsf(cm - 0.5f) >= fabsf(bm - 0.5f)) {
                    for (int ch = 0; ch < 3 && !err; ++ch) {
                        float dA = (ch == 0) ? (c[1] - c[0]) : (ch == 1) ? (c[2] - c[1]) : (c[0] - c[2]);
                        float dB = (ch == 0) ? (b[1] - b[0]) : (ch == 1) ? (b[2] - b[1]) : (b[0] - b[2]);
                        msdfcc_float_t t_val = (msdfcc_float_t)dA / (msdfcc_float_t)(dA - dB);
                        if (t_val > (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON && t_val < MSDFCC_LIT(1.0) - (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON) {
                            float xm = interpolated_median_linear(c, b, t_val);
                            int flags = ec_range_test(v_span, protected_flag, 0, 1, t_val, cm, bm, xm);
                            if (flags & 2) err = 1; else if (flags & 1 && shape_distance_evaluate(bitmap, size, shape, &combiner, edge_cache, xform, min_improve_ratio, sdf_x, sdf_y, shape_coord, 0, -1, t_val, c, a_psd)) err = 1;
                        }
                    }
                }
            }
            if (!err && x + 1 < width) {
                const float* r = (const float*)(bitmap + y * row_stride + x + 1);
                float rm = median3f(r[0], r[1], r[2]);
                if (fabsf(cm - 0.5f) >= fabsf(rm - 0.5f)) {
                    for (int ch = 0; ch < 3 && !err; ++ch) {
                        float dA = (ch == 0) ? (c[1] - c[0]) : (ch == 1) ? (c[2] - c[1]) : (c[0] - c[2]);
                        float dB = (ch == 0) ? (r[1] - r[0]) : (ch == 1) ? (r[2] - r[1]) : (r[0] - r[2]);
                        msdfcc_float_t t_val = (msdfcc_float_t)dA / (msdfcc_float_t)(dA - dB);
                        if (t_val > (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON && t_val < MSDFCC_LIT(1.0) - (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON) {
                            float xm = interpolated_median_linear(c, r, t_val);
                            int flags = ec_range_test(h_span, protected_flag, 0, 1, t_val, cm, rm, xm);
                            if (flags & 2) err = 1; else if (flags & 1 && shape_distance_evaluate(bitmap, size, shape, &combiner, edge_cache, xform, min_improve_ratio, sdf_x, sdf_y, shape_coord, 1, 0, t_val, c, a_psd)) err = 1;
                        }
                    }
                }
            }
            if (!err && y + 1 < height) {
                const float* t = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x);
                float tm = median3f(t[0], t[1], t[2]);
                if (fabsf(cm - 0.5f) >= fabsf(tm - 0.5f)) {
                    for (int ch = 0; ch < 3 && !err; ++ch) {
                        float dA = (ch == 0) ? (c[1] - c[0]) : (ch == 1) ? (c[2] - c[1]) : (c[0] - c[2]);
                        float dB = (ch == 0) ? (t[1] - t[0]) : (ch == 1) ? (t[2] - t[1]) : (t[0] - t[2]);
                        msdfcc_float_t t_val = (msdfcc_float_t)dA / (msdfcc_float_t)(dA - dB);
                        if (t_val > (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON && t_val < MSDFCC_LIT(1.0) - (msdfcc_float_t)MSDFCC_EC_ARTIFACT_T_EPSILON) {
                            float xm = interpolated_median_linear(c, t, t_val);
                            int flags = ec_range_test(v_span, protected_flag, 0, 1, t_val, cm, tm, xm);
                            if (flags & 2) err = 1; else if (flags & 1 && shape_distance_evaluate(bitmap, size, shape, &combiner, edge_cache, xform, min_improve_ratio, sdf_x, sdf_y, shape_coord, 0, 1, t_val, c, a_psd)) err = 1;
                        }
                    }
                }
            }
            if (!err && x > 0 && y > 0) {
                const float* l = (const float*)(bitmap + y * row_stride + x - 1);
                const float* b = (const float*)(bitmap + (size_t)(y - 1) * row_stride + x);
                const float* lb = (const float*)(bitmap + (size_t)(y - 1) * row_stride + x - 1);
                err = ec_diagonal_artifact_with_shape(bitmap, size, shape, &combiner, edge_cache, xform, min_deviation_ratio, min_improve_ratio, c, l, b, lb, cm, d_span, protected_flag, sdf_x, sdf_y, shape_coord, -1, -1);
            }
            if (!err && x + 1 < width && y > 0) {
                const float* r = (const float*)(bitmap + y * row_stride + x + 1);
                const float* b = (const float*)(bitmap + (size_t)(y - 1) * row_stride + x);
                const float* rb = (const float*)(bitmap + (size_t)(y - 1) * row_stride + x + 1);
                err = ec_diagonal_artifact_with_shape(bitmap, size, shape, &combiner, edge_cache, xform, min_deviation_ratio, min_improve_ratio, c, r, b, rb, cm, d_span, protected_flag, sdf_x, sdf_y, shape_coord, 1, -1);
            }
            if (!err && x > 0 && y + 1 < height) {
                const float* l = (const float*)(bitmap + y * row_stride + x - 1);
                const float* t = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x);
                const float* lt = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x - 1);
                err = ec_diagonal_artifact_with_shape(bitmap, size, shape, &combiner, edge_cache, xform, min_deviation_ratio, min_improve_ratio, c, l, t, lt, cm, d_span, protected_flag, sdf_x, sdf_y, shape_coord, -1, 1);
            }
            if (!err && x + 1 < width && y + 1 < height) {
                const float* r = (const float*)(bitmap + y * row_stride + x + 1);
                const float* t = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x);
                const float* rt = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x + 1);
                err = ec_diagonal_artifact_with_shape(bitmap, size, shape, &combiner, edge_cache, xform, min_deviation_ratio, min_improve_ratio, c, r, t, rt, cm, d_span, protected_flag, sdf_x, sdf_y, shape_coord, 1, 1);
            }
            if (err) stencil[y * row_stride + x] |= MSDFCC_EC_STENCIL_ERROR;
        }
        x_direction = -x_direction;
    }

    msdfcc_free(edge_cache);
    msdfcc_mtsdf_overlap_contour_combiner_uninit(&combiner);
}

// reference: MSDFErrorCorrection::findErrors (SDF only)
static void ec_find_errors(
    const msdfcc_rgba_t* bitmap, unsigned char* stencil,
    msdfcc_size2d_t size,
    const msdfcc_sdf_transformation_t* xform, msdfcc_float_t min_deviation_ratio) noexcept {
    const int width = size.width;
    const int height = size.height;
    const int row_stride = msdfcc_size2d_row_stride(size);
    const msdfcc_projection_t* proj = &xform->projection;
    const msdfcc_distance_mapping_t* dm = &xform->distance_mapping;
    msdfcc_float_t delta1 = distance_mapping_delta(dm, MSDFCC_LIT(1.0));

    msdfcc_point_t h = projection_unproject_vector(proj, delta1, MSDFCC_LIT(0));
    msdfcc_point_t v = projection_unproject_vector(proj, MSDFCC_LIT(0), delta1);
    msdfcc_point_t d = projection_unproject_vector(proj, delta1, delta1);
    msdfcc_float_t h_span = min_deviation_ratio * msdfcc_sqrt(h.x * h.x + h.y * h.y);
    msdfcc_float_t v_span = min_deviation_ratio * msdfcc_sqrt(v.x * v.x + v.y * v.y);
    msdfcc_float_t d_span = min_deviation_ratio * msdfcc_sqrt(d.x * d.x + d.y * d.y);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float* c = (const float*)(bitmap + y * row_stride + x);
            float cm = median3f(c[0], c[1], c[2]);
            int prot = (stencil[y * row_stride + x] & MSDFCC_EC_STENCIL_PROTECTED) != 0;
            bool err = false;
            if (x > 0) {
                const float* l = (const float*)(bitmap + y * row_stride + x - 1);
                err |= has_linear_artifact(h_span, prot, cm, c, l);
            }
            if (y > 0) {
                const float* b = (const float*)(bitmap + (size_t)(y - 1) * row_stride + x);
                err |= has_linear_artifact(v_span, prot, cm, c, b);
            }
            if (x + 1 < width) {
                const float* r = (const float*)(bitmap + y * row_stride + x + 1);
                err |= has_linear_artifact(h_span, prot, cm, c, r);
            }
            if (y + 1 < height) {
                const float* t = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x);
                err |= has_linear_artifact(v_span, prot, cm, c, t);
            }
            if (x > 0 && y > 0) {
                const float* l = (const float*)(bitmap + y * row_stride + x - 1);
                const float* b = (const float*)(bitmap + (size_t)(y - 1) * row_stride + x);
                const float* lb = (const float*)(bitmap + (size_t)(y - 1) * row_stride + x - 1);
                err |= has_diagonal_artifact(d_span, prot, cm, c, l, b, lb);
            }
            if (x + 1 < width && y > 0) {
                const float* r = (const float*)(bitmap + y * row_stride + x + 1);
                const float* b = (const float*)(bitmap + (size_t)(y - 1) * row_stride + x);
                const float* rb = (const float*)(bitmap + (size_t)(y - 1) * row_stride + x + 1);
                err |= has_diagonal_artifact(d_span, prot, cm, c, r, b, rb);
            }
            if (x > 0 && y + 1 < height) {
                const float* l = (const float*)(bitmap + y * row_stride + x - 1);
                const float* t = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x);
                const float* lt = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x - 1);
                err |= has_diagonal_artifact(d_span, prot, cm, c, l, t, lt);
            }
            if (x + 1 < width && y + 1 < height) {
                const float* r = (const float*)(bitmap + y * row_stride + x + 1);
                const float* t = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x);
                const float* rt = (const float*)(bitmap + (size_t)(y + 1) * row_stride + x + 1);
                err |= has_diagonal_artifact(d_span, prot, cm, c, r, t, rt);
            }
            if (err) stencil[y * row_stride + x] |= MSDFCC_EC_STENCIL_ERROR;
        }
    }
}

// reference: MSDFErrorCorrection::apply - set r=g=b=median for ERROR pixels
static void ec_apply(
    msdfcc_rgba_t* bitmap, const unsigned char* stencil,
    msdfcc_size2d_t size) noexcept {
    const int width = size.width;
    const int height = size.height;
    const int row_stride = msdfcc_size2d_row_stride(size);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (stencil[y * row_stride + x] & MSDFCC_EC_STENCIL_ERROR) {
                msdfcc_rgba_t* p = bitmap + y * row_stride + x;
                float m = median3f(p->r, p->g, p->b);
                p->r = p->g = p->b = m;
            }
        }
    }
}

// reference: msdf-error-correction.cpp msdfErrorCorrection
void
msdfcc_msdf_error_correction_overlap(
    msdfcc_rgba_t* bitmap, msdfcc_size2d_t size,
    const msdfcc_shape_t* shape, const msdfcc_sdf_transformation_t* transformation,
    const msdfcc_error_correction_config_t* config) noexcept {
    assert(shape && "shape must be non-NULL");
    if (!bitmap || !transformation || !config || size.width == 0 || size.height == 0)
        return;
    if (config->mode == msdfcc_ec_mode_disabled)
        return;

    const int width = size.width;
    const int height = size.height;
    size_t stencil_size = (size_t)width * height;
    unsigned char* stencil = config->buffer;
    unsigned char* stencil_alloc = NULL;
    if (!stencil) {
        stencil_alloc = (unsigned char*)msdfcc_malloc(stencil_size);
        if (!stencil_alloc) return;
        stencil = stencil_alloc;
    }
    memset(stencil, 0, stencil_size);

    switch (config->mode)
    {
    case msdfcc_ec_mode_disabled:
    case msdfcc_ec_mode_indiscriminate:
        break;
    case msdfcc_ec_mode_edge_priority:
        ec_protect_corners(shape, stencil, size, transformation);
        ec_protect_edges(bitmap, stencil, size, transformation);
        break;
    case msdfcc_ec_mode_edge_only:
        ec_protect_all(stencil, size);
        break;
    }

    // reference: msdf-error-correction.cpp msdfErrorCorrectionInner
    if (config->distance_check_mode == msdfcc_ec_distance_check_do_not ||
        (config->distance_check_mode == msdfcc_ec_distance_check_at_edge && config->mode != msdfcc_ec_mode_edge_only)) {
        ec_find_errors(bitmap, stencil, size, transformation, config->min_deviation_ratio);
        if (config->distance_check_mode == msdfcc_ec_distance_check_at_edge)
            ec_protect_all(stencil, size);
    }
    if (config->distance_check_mode == msdfcc_ec_distance_check_always ||
        config->distance_check_mode == msdfcc_ec_distance_check_at_edge) {
        // overlap_support
        ec_find_errors_with_shape_overlap(bitmap, stencil, size, shape, transformation, config->min_deviation_ratio, config->min_improve_ratio);
    }

    ec_apply(bitmap, stencil, size);

    msdfcc_free(stencil_alloc);
}

void msdfcc_generate_mtsdf_overlap(msdfcc_rgba_t* bitmap, msdfcc_size2d_t size, const msdfcc_shape_t* shape, const msdfcc_sdf_transformation_t* transformation, const msdfcc_error_correction_config_t* config) noexcept
{
    generate_distance_field_mtsdf_overlap(shape, bitmap, size, transformation);
    msdfcc_msdf_error_correction_overlap(bitmap, size, shape, transformation, config);
}


static inline void 
swap_intersection(scanline_intersection_t* a, scanline_intersection_t* b) noexcept {
    scanline_intersection_t tmp = *a;
    *a = *b;
    *b = tmp;
}

static void 
insertion_sort_intersections(scanline_intersection_t* arr, int n) noexcept {
    for (int i = 1; i < n; i++) {
        scanline_intersection_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j].x > key.x) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static inline int 
median_of_three(scanline_intersection_t* arr, int lo, int hi) noexcept {
    int mid = lo + (hi - lo) / 2;
    if (arr[mid].x < arr[lo].x)
        swap_intersection(&arr[mid], &arr[lo]);
    if (arr[hi].x < arr[lo].x)
        swap_intersection(&arr[hi], &arr[lo]);
    if (arr[mid].x < arr[hi].x)
        swap_intersection(&arr[mid], &arr[hi]);
    return hi;
}

static int 
partition_intersections(scanline_intersection_t* arr, int lo, int hi) noexcept {
    median_of_three(arr, lo, hi);
    msdfcc_float_t pivot = arr[hi].x;

    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        if (arr[j].x <= pivot) {
            i++;
            swap_intersection(&arr[i], &arr[j]);
        }
    }
    swap_intersection(&arr[i + 1], &arr[hi]);
    return i + 1;
}

static void 
qsort_intersections_impl(scanline_intersection_t* arr, int lo, int hi) noexcept {
    while (lo < hi) {
        if (hi - lo < 16) {
            insertion_sort_intersections(arr + lo, hi - lo + 1);
            return;
        }

        int p = partition_intersections(arr, lo, hi);

        if (p - lo < hi - p) {
            qsort_intersections_impl(arr, lo, p - 1);
            lo = p + 1;
        }
        else {
            qsort_intersections_impl(arr, p + 1, hi);
            hi = p - 1;
        }
    }
}

static void 
qsort_intersections(scanline_intersection_t* arr, int n) noexcept {
    if (n > 1) {
        qsort_intersections_impl(arr, 0, n - 1);
    }
}

#ifdef __cplusplus
}
#endif
#endif