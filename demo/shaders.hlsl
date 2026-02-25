Texture2D shaderTexture;
SamplerState SampleType;
// 2d matrix
cbuffer AffineMatrix2D : register (b0) {
    float4      m12;    // [m11, m21, m12, m22]
    float4      dxy;    // [dx, dy, -, -]
    float4      screen; // [w,h,1/w,1/h]
    float4      texParams; // [w,h,1/w,1/h]
};
// 3d matrix
cbuffer PolygonBufferType : register (b0) {
    matrix worldMatrix;
    matrix worldInvMatrix;
    matrix worldViewProjMatrix;
    //matrix viewMatrix;
    //matrix projectionMatrix;
    float4 screenPolygon; // [w,h,1/w,1/h]
    float4 color;
    float4 lightDirection;
    //float4 ambientLight;
    //float4 diffuseColor; 
};

float2 affine(float2 pt) {
    return mul(float2x2(m12), pt) + dxy.xy;
}

struct VertexInputType {
    float4 position     : POSITION;
    float4 tex          : TEXCOORD0;
    uint4  color1       : COLOR0;
    uint4  color2       : COLOR1;

    //uint   vertexID     : SV_VertexID;
};

struct GrayVertexInputType {
    float2 position     : POSITION;
    float2 tex          : TEXCOORD0;
    uint4  color        : COLOR0;
};
// 灰度实例化：单位四边形(per-vertex) + 实例数据(per-instance)，与 MSDF 一致
// rect 为原始 fp26.6 (int4)；srcRect 为纹理内像素 (x,y,w,h) 16bit，在 VS 中用 texParams 转归一化 UV
struct GrayInstancedVertexInputType {
    float2 position     : POSITION;   // slot0 单位四边形 0~1
    float2 tex          : TEXCOORD0;  // slot0
    int4   rect         : TEXCOORD1;  // slot1 实例: dstX,dstY,dstW,dstH (fp26.6)
    uint4  srcRect      : TEXCOORD2;  // slot1 实例: srcX,srcY,srcW,srcH (像素, 16bit)
    uint4  color        : COLOR1;     // slot1 实例
};

struct RectVertexInputType {
    float2 position     : POSITION;
    uint4  color        : COLOR0;
};
// 纯色矩形实例化：单位四边形(per-vertex) + 实例数据(per-instance)
struct RectInstancedVertexInputType {
    float2 position     : POSITION;   // slot0 单位四边形 0~1
    float4 rect         : TEXCOORD1; // slot1 实例: x,y,w,h
    uint4  color        : COLOR1;    // slot1 实例
};

// MSDF 实例化：单位四边形(per-vertex) + 实例数据(per-instance)，color1/color2/range
// rect 为原始 fp26.6 (int4)；srcRect 为纹理内像素 (x,y,w,h) 16bit，在 VS 中用 texParams 转归一化 UV
struct MsdfInstancedVertexInputType {
    float2 position     : POSITION;   // slot0 单位四边形 0~1
    float2 tex          : TEXCOORD0;  // slot0
    int4   rect         : TEXCOORD1;  // slot1 实例: dstX,dstY,dstW,dstH (fp26.6)
    uint4  srcRect      : TEXCOORD2;  // slot1 实例: srcX,srcY,srcW,srcH (像素, 16bit)
    uint4  color1       : COLOR0;     // slot1 实例
    uint4  color2       : COLOR1;     // slot1 实例
    int    glyph        : TEXCOORD3;  // slot1 实例 fp26.6
};

struct PolygonVertexInputType {
    float4 position     : POSITION;
    float4 normal       : NORMAL0;
    uint index          : BLENDINDICES0;
};

struct PolygonGeometryInputType {

    //float4 position         : SV_POSITION;

    float3 normal           : NORMAL;

    float4 color            : COLOR0;
    //float4 ambientLight   : COLOR0;
    //float4 diffuseColor   : COLOR1;

    float4 lightDirection   : TEXCOORD0;


    float4 posH             : POSITION0;
    float4 posExtrudedH     : POSITION1;

    float4 posW             : POSITION2;
    float4 posExtrudedW     : POSITION4;

    int index : BLENDINDICES0;
};


struct PolygonPixelInputType {

    float4 position         : SV_POSITION;
    float3 normal           : NORMAL;
    float4 lightDirection   : TEXCOORD0;

    float4 color            : COLOR0;
    //float4 ambientLight  : COLOR0;
    //float4 diffuseColor  : COLOR1;


};

PolygonGeometryInputType vs_polygon(PolygonVertexInputType input) {
    PolygonGeometryInputType output;
#if 1
    output.posW = mul(float4(input.position.xy, 0, 1), worldMatrix);
    output.posH = mul(float4(input.position.xy, 0, 1), worldViewProjMatrix);


    float hight = 0.3f;
    output.posExtrudedW = mul(float4(input.position.xy, hight, 1), worldMatrix);
    output.posExtrudedH = mul(float4(input.position.xy, hight, 1), worldViewProjMatrix);

    output.normal = mul(float4(input.normal.xy, 0, 1), worldInvMatrix).xyz;

    output.lightDirection = lightDirection;
    output.color = color;
    output.index = input.index;

#else
    // affine transformation
    float2 pos = affine(input.position.xy);
    // screen coordinates
    pos = float2(pos.x * screen.z * 2.0 - 1.0, 1.0 - pos.y * screen.w * 2.0);

    output.position = float4(pos.xy, 0.0, 1.0);
#endif

    return output;
}


float4 ps_polygon(PolygonPixelInputType input) : SV_TARGET {
    float4 ambientColor = float4(0.1f, 0.1f, 0.1f, 1.0f);
    float4 diffuseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);


    float3 norm = normalize(input.normal);
    float3 lightDir = normalize(input.lightDirection).xyz;

    float diff = max(dot(norm, lightDir), 0.0);
    float4 diffuse = diff * diffuseColor;

    float4 result = (ambientColor + diffuse) * input.color;

    return result;

};

float3 cal_normal(float4 a, float4 b, float4 c) {
    float3 edge1 = a.xyz - b.xyz;
    float3 edge2 = c.xyz - b.xyz;
    return cross(edge1, edge2);
}

// L  GGGG...G IIII..I
// 31 21....30 0...20
bool is_edge(int a, int b) {

    //return true;
    int lowA = a & ((1 << 21) - 1);
    int lowB = b & ((1 << 21) - 1);

    int midA = a & 0x7fffffff;
    int midB = b & 0x7fffffff;

    int highA = a & ~((1 << 21) - 1);
    int highB = b & ~((1 << 21) - 1);

    return (abs(midA - midB) == 1) || (lowA * lowB == 0 && (highA^highB) == 0x80000000);
 
}

bool non_zero(float3 a, float3 b) {
    float left = a.x + a.y + a.z;
    float right = b.x + b.y + b.z;
    return left * right != 0;
}

[maxvertexcount(24)]
void gs_extrude(triangle PolygonGeometryInputType input[3], inout TriangleStream<PolygonPixelInputType> stream)
{
    PolygonPixelInputType unit;

    unit.lightDirection = input[0].lightDirection;
    unit.color = input[0].color;

    // TRI#0 [TOP]
    unit.normal = cal_normal(input[0].posW, input[1].posW, input[2].posW);

    unit.position = input[0].posH; stream.Append(unit);
    unit.position = input[1].posH; stream.Append(unit);
    unit.position = input[2].posH; stream.Append(unit);
    stream.RestartStrip();

    // TRI#1 [BOTTOM]
    unit.normal = cal_normal(input[0].posExtrudedW, input[2].posExtrudedW, input[1].posExtrudedW);

    unit.position = input[0].posExtrudedH; stream.Append(unit);
    unit.position = input[2].posExtrudedH; stream.Append(unit);
    unit.position = input[1].posExtrudedH; stream.Append(unit);
    stream.RestartStrip();
    // 0-1
    if (is_edge(input[0].index, input[1].index)) {
        // NORMAL
        float3 normal = cal_normal(input[0].posExtrudedW, input[1].posExtrudedW, input[0].posW);
        float3 normal0, normal1;
        if (non_zero(input[0].normal, input[1].normal)) {
            normal0 = input[0].normal;
            normal1 = input[1].normal;
        }
        else {
            normal0 = normal;
            normal1 = normal;
        }

        // TRI#2 AB0
        unit.normal = normal0;  unit.position = input[0].posExtrudedH;  stream.Append(unit);
        unit.normal = normal1;  unit.position = input[1].posExtrudedH;  stream.Append(unit);
        unit.normal = normal0;  unit.position = input[0].posH;          stream.Append(unit);
        stream.RestartStrip();

        // TRI#3 0B1
        unit.normal = normal0;  unit.position = input[0].posH;          stream.Append(unit);
        unit.normal = normal1;  unit.position = input[1].posExtrudedH;  stream.Append(unit);
        unit.normal = normal1;  unit.position = input[1].posH;          stream.Append(unit);
        stream.RestartStrip();
    }

    // 1-2
    if (is_edge(input[1].index, input[2].index)) {
        // NORMAL
        float3 normal = cal_normal(input[1].posExtrudedW, input[2].posExtrudedW, input[1].posW);
        float3 normal1, normal2;
        if (non_zero(input[1].normal, input[2].normal)) {
            normal1 = input[1].normal;
            normal2 = input[2].normal;
        }
        else {
            normal1 = normal;
            normal2 = normal;
        }

        // TRI#4 BC1
        unit.normal = normal1;  unit.position = input[1].posExtrudedH;  stream.Append(unit);
        unit.normal = normal2;  unit.position = input[2].posExtrudedH;  stream.Append(unit);
        unit.normal = normal1;  unit.position = input[1].posH;          stream.Append(unit);
        stream.RestartStrip();

        // TRI#5 1C2
        unit.normal = normal1;  unit.position = input[1].posH; stream.Append(unit);
        unit.normal = normal2;  unit.position = input[2].posExtrudedH; stream.Append(unit);
        unit.normal = normal2;  unit.position = input[2].posH; stream.Append(unit);
        stream.RestartStrip();
    }

    // 0-2
    if (is_edge(input[0].index, input[2].index)) {
        // NORMAL
        float3 normal = cal_normal(input[2].posExtrudedW, input[0].posExtrudedW, input[2].posW);
        float3 normal0, normal2;
        if (non_zero(input[0].normal, input[2].normal)) {
            normal0 = input[0].normal;
            normal2 = input[2].normal;
        }
        else {
            normal0 = normal;
            normal2 = normal;
        }

        // TRI#6 CA2
        unit.normal = normal2;  unit.position = input[2].posExtrudedH;  stream.Append(unit);
        unit.normal = normal0;  unit.position = input[0].posExtrudedH;  stream.Append(unit);
        unit.normal = normal2;  unit.position = input[2].posH;          stream.Append(unit);
        stream.RestartStrip();

        // TRI#7 2A0
        unit.normal = normal2;  unit.position = input[2].posH;          stream.Append(unit);
        unit.normal = normal0;  unit.position = input[0].posExtrudedH;  stream.Append(unit);
        unit.normal = normal0;  unit.position = input[0].posH;          stream.Append(unit);
        stream.RestartStrip();
    }
}



struct PixelInputType {
    float4 position     : SV_POSITION;
    float4 tex          : TEXCOORD0;
    float4 color1       : COLOR0;
    float4 color2       : COLOR1;
    //float4 data         : COLOR2;
};


//float get_opacity_up(float range) {
//    return saturate(2 - range) + 1;
//}

float4 color_u2f(uint4 c) {
    return float4(float(c.x) / 255.0f,c.y / 255.0f, c.z / 255.0f, c.w / 255.0f);
}

PixelInputType vs_main(VertexInputType input) {
    PixelInputType output;
    output.color1 = color_u2f(input.color1);
    output.color2 = color_u2f(input.color2);

    // affine transformation
    float2 pos = affine(input.position.xy);
    // screen coordinates
    pos = float2(pos.x * screen.z * 2.0 - 1.0, 1.0 - pos.y * screen.w * 2.0);

    output.position = float4(pos.xy, 0.0, 1.0);

    // range
    output.tex = input.tex;
    float scale = max(length(m12.xy), length(m12.zw));
    float range = scale * output.tex.w;
    output.tex.w = max(range, 1);

    //output.data = float4(get_opacity_up(range), 1, 1, 1);

    return output;
}

float2 align_to(float2 value, float4 aligned) {
    return round(value * aligned.xy) * aligned.zw;
}

PixelInputType vs_pixel(VertexInputType input) {
    PixelInputType output = vs_main(input);
    output.position.xy = align_to(output.position.xy, screen * float4(0.5,0.5,2,2));
    return output;
}

// 灰度实例化：rect 为 fp26.6，/64 转 float；srcRect 为像素，用 texParams.zw 转归一化 UV
PixelInputType vs_gray(GrayInstancedVertexInputType input) {
    PixelInputType output;
    output.color1 = color_u2f(input.color);
    output.color2 = float4(0, 0, 0, 0);

    // 实例 rect 为 fp26.6，转成 float 后计算世界坐标
    float2 rectXY = float2(input.rect.x, input.rect.y) * (1.0 / 64.0);
    float2 rectWH = float2(input.rect.z, input.rect.w) * (1.0 / 64.0);
    float2 worldPos = rectXY + input.position.xy * rectWH;
    float2 pos = affine(worldPos);
    pos = float2(pos.x * screen.z * 2.0 - 1.0, 1.0 - pos.y * screen.w * 2.0);
    output.position = float4(pos.xy, 0.0, 1.0);

    // 实例 srcRect = 像素 (x,y,w,h)，用 texParams.zw=(1/w,1/h) 转归一化 UV
    float2 uv0 = float2(input.srcRect.x, input.srcRect.y) * texParams.zw;
    float2 uv1 = float2(input.srcRect.x + input.srcRect.z, input.srcRect.y + input.srcRect.w) * texParams.zw;
    float2 tex = lerp(uv0, uv1, input.tex.xy);
    output.tex = float4(tex.xy, 0.0, 0.0);

    return output;
}

// MSDF 实例化：使用 color1/color2/range 实例数据（rect 为 fp26.6，/64 转 float）
PixelInputType vs_msdf_instanced(MsdfInstancedVertexInputType input) {
    PixelInputType output;
    output.color1 = color_u2f(input.color1);
    output.color2 = color_u2f(input.color2);

    // 实例 rect 为 fp26.6，转成 float 后计算世界坐标；单位四边形 position 为 (0,0)(1,0)(1,1)(0,1)
    float2 rectXY = float2(input.rect.x, input.rect.y) * (1.0 / 64.0);
    float2 rectWH = float2(input.rect.z, input.rect.w) * (1.0 / 64.0);
    float2 worldPos = rectXY + input.position.xy * rectWH;
    float2 pos = affine(worldPos);
    pos = float2(pos.x * screen.z * 2.0 - 1.0, 1.0 - pos.y * screen.w * 2.0);
    output.position = float4(pos.xy, 0.0, 1.0);

    // 实例 srcRect = 像素 (x,y,w,h)，用 texParams.zw=(1/w,1/h) 转归一化 UV；单位 tex 为 (0,0)(1,0)(1,1)(0,1)
    float2 uv0 = float2(input.srcRect.x, input.srcRect.y) * texParams.zw;
    float2 uv1 = float2(input.srcRect.x + input.srcRect.z, input.srcRect.y + input.srcRect.w) * texParams.zw;
    float2 tex = lerp(uv0, uv1, input.tex.xy);

    // 实例 range 用于 MSDF 距离场
    //float scale = max(length(m12.xy), length(m12.zw));
    float scale = 1.0;
    //           scale     glyph-size      fp26.6     base-range     texture-size
    float range = scale * input.glyph * (1.0 / 64.0) * 4.0    /   float(input.srcRect.z);
    output.tex = float4(tex.xy, 0.0, max(range, 1.0));
    return output;
}


float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}


//float opacity_up(float opacity, float4 data) {
//    return saturate(opacity * data.x);
//}

#if 0
// ONLY FOR WHITE ON BLACK?
float opacity_up(float opacity) {
    float gamma = 1.8;
    return pow(opacity, 1.0 / gamma);
}
float3 opacity_up3(float3 opacity) {
    float gamma = 1.8;
    return pow(opacity, 1.0 / gamma);
}
#else
// SRGB
float opacity_up(float opacity) {
    return opacity;
}
float3 opacity_up3(float3 opacity) {
    return opacity;
}
#endif


float4 ps_msdf(PixelInputType input) : SV_TARGET {
    float4 color = input.color1;
    float4 data = shaderTexture.Sample(SampleType, input.tex.xy);
    float sd = median(data.r, data.g, data.b);
    float screenPxDistance = input.tex.w * (sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    color.a *= opacity_up(opacity/*, input.data*/);
    return color;
}


float4 ps_gray(PixelInputType input) : SV_TARGET {
    float4 color = input.color1;
    float4 data = shaderTexture.Sample(SampleType, input.tex.xy);

    color.a *= opacity_up(data.r/*, input.data*/);
    return color;
}

//  子像素渲染 + sRGB 混合 字体过细
// https://www.puredevsoftware.com/blog/2019/01/22/sub-pixel-gamma-correct-font-rendering/
float4 subpixel_srgb_gamma(float4 value)
{
    const float gammaCorrection = 1.4;
    return saturate(pow(value, 1.0 / gammaCorrection));
}

struct SubpixelOutput {
    float4 color      : SV_Target0;
    float4 mask       : SV_Target1;
};

SubpixelOutput ps_subh(PixelInputType input) {
    float4 color = input.color1;
    float4 data = shaderTexture.Sample(SampleType, input.tex.xy);

    SubpixelOutput output;
    output.color = color;
    output.mask = (data);
    return output;
}

float4 ps_color(PixelInputType input) : SV_TARGET {
    return input.color1;
};

PixelInputType vs_rect_fill(RectVertexInputType input) {
    PixelInputType output;
    output.color1 = color_u2f(input.color);
    output.color2 = float4(0, 0, 0, 0);

    // affine transformation
    float2 pos = affine(input.position.xy);
    // screen coordinates
    pos = float2(pos.x * screen.z * 2.0 - 1.0, 1.0 - pos.y * screen.w * 2.0);

    output.position = float4(pos.xy, 0.0, 1.0);

    // 不需要纹理坐标
    output.tex = float4(0.0, 0.0, 0.0, 0.0);

    return output;
}

// 纯色矩形实例化：单位四边形顶点 + 实例 rect/color
PixelInputType vs_rect_fill_instanced(RectInstancedVertexInputType input) {
    PixelInputType output;
    output.color1 = color_u2f(input.color);
    output.color2 = float4(0, 0, 0, 0);

    // 实例 rect = (x,y,w,h)，单位四边形 position 为 (0,0)(1,0)(1,1)(0,1)
    float2 worldPos = input.rect.xy + input.position.xy * input.rect.zw;
    float2 pos = affine(worldPos);
    pos = float2(pos.x * screen.z * 2.0 - 1.0, 1.0 - pos.y * screen.w * 2.0);
    output.position = float4(pos.xy, 0.0, 1.0);

    output.tex = float4(0.0, 0.0, 0.0, 0.0);
    return output;
}

float4 ps_rect_fill(PixelInputType input) : SV_TARGET {
    return input.color1;
};
