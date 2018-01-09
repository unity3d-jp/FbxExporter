#include "pch.h"
#include "MeshUtils/MeshUtils.h"
#include "FbxExporter.h"
#include "fbxeContext.h"

using namespace fbxe;

fbxeAPI fbxe::IContext* fbxeCreateContext(const fbxe::ExportOptions *opt)
{
    return fbxe::CreateContext(opt);
}

fbxeAPI void fbxeReleaseContext(fbxe::IContext *ctx)
{
    if (!ctx) { return; }
    ctx->release();
}

fbxeAPI int fbxeCreateScene(fbxe::IContext *ctx, const char *name)
{
    if (!ctx) { return false; }
    return ctx->createScene(name);
}

fbxeAPI int fbxeWriteAsync(fbxe::IContext *ctx, const char *path, fbxe::Format format)
{
    if (!ctx) { return false; }
    return ctx->writeAsync(path, format);
}

fbxeAPI int fbxeIsFinished(fbxe::IContext *ctx)
{
    if (!ctx) { return false; }
    return ctx->isFinished();
}

fbxeAPI fbxe::Node* fbxeGetRootNode(fbxe::IContext *ctx)
{
    if (!ctx) { return nullptr; }
    return ctx->getRootNode();
}
fbxeAPI fbxe::Node* fbxeFindNodeByName(fbxe::IContext *ctx, const char *name)
{
    if (!ctx) { return nullptr; }
    return ctx->findNodeByName(name);
}

fbxeAPI fbxe::Node* fbxeCreateNode(fbxe::IContext *ctx, fbxe::Node *parent, const char *name)
{
    if (!ctx) { return nullptr; }
    return ctx->createNode(parent, name);
}

fbxeAPI void fbxeSetTRS(fbxe::IContext *ctx, fbxe::Node *node, float3 t, quatf r, float3 s)
{
    if (!ctx) { return; }
    ctx->setTRS(node, t, r, s);
}

fbxeAPI void fbxeAddMesh(fbxe::IContext *ctx, fbxe::Node *node, int num_vertices,
    const float3 points[], const float3 normals[], const float4 tangents[], const float2 uv[], const float4 colors[])
{
    if (!ctx) { return; }
    ctx->addMesh(node, num_vertices, points, normals, tangents, uv, colors);
}

fbxeAPI void fbxeAddMeshSubmesh(fbxe::IContext *ctx, fbxe::Node *node, fbxe::Topology topology, int num_indices, const int indices[], int material)
{
    if (!ctx) { return; }
    ctx->addMeshSubmesh(node, topology, num_indices, indices, material);
}

fbxeAPI void fbxeAddMeshSkin(fbxe::IContext *ctx, fbxe::Node *node, Weights4 weights[], int num_bones, fbxe::Node *bones[], float4x4 bindposes[])
{
    if (!ctx) { return; }
    ctx->addMeshSkin(node, weights, num_bones, bones, bindposes);
}

fbxeAPI void fbxeAddMeshBlendShape(fbxe::IContext *ctx, fbxe::Node *node, const char *name, float weight,
    const fbxe::float3 delta_points[], const fbxe::float3 delta_normals[], const fbxe::float3 delta_tangents[])
{
    if (!ctx) { return; }
    ctx->addMeshBlendShape(node, name, weight, delta_points, delta_normals, delta_tangents);
}



fbxeAPI void fbxeGenerateTerrainMesh(
    const float heightmap[], int width, int height, float3 size,
    float3 dst_vertices[], float3 dst_normals[], float2 dst_uv[], int dst_indices[])
{
    int num_vertices = width * height;
    int num_triangles = (width - 1) * (height - 1) * 2;
    auto size_unit = float3{ 1.0f / (width - 1), 1.0f, 1.0f / (height - 1) } *size;
    auto uv_unit = float2{ 1.0f / (width - 1), 1.0f / (height - 1) };

    parallel_invoke(
        [&]() {
        for (int iy = 0; iy < height; ++iy) {
            for (int ix = 0; ix < width; ++ix) {
                int i = iy * width + ix;
                dst_vertices[i] = float3{ (float)ix, heightmap[i], (float)iy } *size_unit;
                dst_uv[i] = float2{ (float)ix, (float)iy } *uv_unit;
            }
        }
    },
        [&]() {
        for (int iy = 0; iy < height - 1; ++iy) {
            for (int ix = 0; ix < width - 1; ++ix) {
                int i6 = (iy * width + ix) * 6;
                dst_indices[i6 + 0] = width*iy + ix;
                dst_indices[i6 + 1] = width*(iy + 1) + ix;
                dst_indices[i6 + 2] = width*(iy + 1) + (ix + 1);

                dst_indices[i6 + 3] = width*iy + ix;
                dst_indices[i6 + 4] = width*(iy + 1) + (ix + 1);
                dst_indices[i6 + 5] = width*iy + (ix + 1);
            }
        }
    });

    GenerateNormalsTriangleIndexed(dst_normals, dst_vertices, dst_indices, num_triangles, num_vertices);
}
