#pragma once

#ifdef _WIN32
    #define fbxeAPI extern "C" __declspec(dllexport)
#else
    #define fbxeAPI extern "C" 
#endif

namespace fbxe {
#ifdef fbxeImpl
    using namespace mu;
#else
    struct float2 { float x, y; };
    struct float3 { float x, y, z; };
    struct float4 { float x, y, z, w; };
    using quatf = float4;
    struct float4x4 { float4 m[4]; };

    template<int N>
    struct Weights
    {
        float   weights[N] = {};
        int     indices[N] = {};
    };
    using Weights4 = Weights<4>;
#endif

    using Node = void;
    class IContext;

    enum class Topology
    {
        Points,
        Lines,
        Triangles,
        Quads,
    };

    enum class SystemUnit
    {
        Millimeter,
        Centimeter,
        Decimeter,
        Meter,
        Kilometer,
    };

    enum class Format
    {
        FbxBinary,
        FbxAscii,
        FbxEncrypted,
        Obj,
    };

    struct ExportOptions
    {
        int flip_handedness = 0;
        int flip_faces = 0;
        int quadify = 1;
        float quadify_threshold_angle = 40.0f;
        float scale_factor = 1.0f;
        SystemUnit system_unit = SystemUnit::Meter;
    };

} // namespace fbxe


fbxeAPI fbxe::IContext* fbxeCreateContext(const fbxe::ExportOptions *opt);
fbxeAPI void            fbxeReleaseContext(fbxe::IContext *ctx);

fbxeAPI int         fbxeCreateScene(fbxe::IContext *ctx, const char *name);
fbxeAPI int         fbxeWriteAsync(fbxe::IContext *ctx, const char *path, fbxe::Format format);
fbxeAPI int         fbxeIsFinished(fbxe::IContext *ctx);

fbxeAPI fbxe::Node* fbxeGetRootNode(fbxe::IContext *ctx);
fbxeAPI fbxe::Node* fbxeFindNodeByName(fbxe::IContext *ctx, const char *name);

fbxeAPI fbxe::Node* fbxeCreateNode(fbxe::IContext *ctx, fbxe::Node *parent, const char *name);
fbxeAPI void        fbxeSetTRS(fbxe::IContext *ctx, fbxe::Node *node, fbxe::float3 t, fbxe::quatf r, fbxe::float3 s);
fbxeAPI void        fbxeAddMesh(fbxe::IContext *ctx, fbxe::Node *node, int num_vertices,
    const fbxe::float3 points[], const fbxe::float3 normals[], const fbxe::float4 tangents[],
    const fbxe::float2 uv[], const fbxe::float4 colors[]);
fbxeAPI void        fbxeAddMeshSubmesh(fbxe::IContext *ctx, fbxe::Node *node, fbxe::Topology topology, int num_indices, const int indices[], int material);
fbxeAPI void        fbxeAddMeshSkin(fbxe::IContext *ctx, fbxe::Node *node, fbxe::Weights4 weights[], int num_bones, fbxe::Node *bones[], fbxe::float4x4 bindposes[]);
fbxeAPI void        fbxeAddMeshBlendShape(fbxe::IContext *ctx, fbxe::Node *node, const char *name, float weight,
    const fbxe::float3 delta_points[], const fbxe::float3 delta_normals[], const fbxe::float3 delta_tangents[]);
