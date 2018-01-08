#include "pch.h"
#include "MeshUtils/MeshUtils.h"
#include "fbxeContext.h"
#include "fbxeUtils.h"

#ifdef _WIN32
    #pragma comment(lib, "libfbxsdk-md.lib")
#endif

namespace fbxe {

class Context : public IContext
{
public:
    Context(const ExportOptions *opt);
    ~Context() override;
    void release() override;
    bool clear() override;

    bool createScene(const char *name) override;
    bool write(const char *path, Format format) override;

    Node* getRootNode() override;
    Node* findNodeByName(const char *name) override;

    Node* createNode(Node *parent, const char *name) override;
    void setTRS(Node *node, float3 t, quatf r, float3 s) override;
    void addMesh(Node *node, int num_vertices,
        const float3 points[], const float3 normals[], const float4 tangents[], const float2 uv[], const float4 colors[]) override;
    void addMeshSubmesh(Node *node, Topology topology, int num_indices, const int indices[], int material) override;
    void addMeshSkin(Node *node, Weights4 weights[], int num_bones, Node *bones[], float4x4 bindposes[]) override;
    void addMeshBlendShape(Node *node, const char *name, float weight,
        const float3 delta_points[], const float3 delta_normals[], const float3 delta_tangents[]) override;

private:
    ExportOptions m_opt;
    FbxManager *m_manager = nullptr;
    FbxScene *m_scene = nullptr;
};

fbxeAPI IContext* CreateContext(const ExportOptions *opt)
{
    return new Context(opt);
}




Context::Context(const ExportOptions *opt)
{
    if (opt) { m_opt = *opt; }
    m_manager = FbxManager::Create();
}

Context::~Context()
{
    clear();
    m_manager->Destroy();
}

void Context::release()
{
    delete this;
}

bool Context::clear()
{
    if (m_scene) {
        m_scene->Destroy(true);
        m_scene = nullptr;
        return true;
    }
    return false;
}

bool Context::createScene(const char *name)
{
    if (!m_manager) { return false; }

    clear();
    m_scene = FbxScene::Create(m_manager, name);
    if (m_scene) {
        auto& settings = m_scene->GetGlobalSettings();
        switch (m_opt.system_unit) {
        case SystemUnit::Millimeter: settings.SetSystemUnit(FbxSystemUnit::mm); break;
        case SystemUnit::Centimeter: settings.SetSystemUnit(FbxSystemUnit::cm); break;
        case SystemUnit::Decimeter: settings.SetSystemUnit(FbxSystemUnit::dm); break;
        case SystemUnit::Meter: settings.SetSystemUnit(FbxSystemUnit::m); break;
        case SystemUnit::Kilometer: settings.SetSystemUnit(FbxSystemUnit::km); break;
        }
    }
    return m_scene != nullptr;
}

bool Context::write(const char *path, Format format)
{
    if (!m_scene) { return false; }

    int file_format = 0;
    {
        // search file format index
        const char *format_name = nullptr;
        switch (format) {
        case Format::FbxBinary: format_name = "FBX binary"; break;
        case Format::FbxAscii: format_name = "FBX ascii"; break;
        case Format::FbxEncrypted: format_name = "FBX encrypted"; break;
        case Format::Obj: format_name = "(*.obj)"; break;
        default: return false;
        }

        int n = m_manager->GetIOPluginRegistry()->GetWriterFormatCount();
        for (int i = 0; i < n; ++i) {
            auto desc = m_manager->GetIOPluginRegistry()->GetWriterFormatDescription(i);
            if (std::strstr(desc, format_name) != nullptr)
            {
                file_format = i;
                break;
            }
        }
    }

    // create exporter
    auto exporter = FbxExporter::Create(m_manager, "");
    if (!exporter->Initialize(path, file_format)) {
        return false;
    }

    // do export
    bool ret = exporter->Export(m_scene);
    exporter->Destroy();
    return ret;
}

Node* Context::getRootNode()
{
    if (!m_scene) { return nullptr; }

    return m_scene->GetRootNode();
}

Node* Context::findNodeByName(const char *name)
{
    if (!m_scene) { return nullptr; }

    int n = m_scene->GetGenericNodeCount();
    for (int i = 0; i < n; ++i) {
        auto node = m_scene->GetGenericNode(i);
        if (std::strcmp(node->GetName(), name) == 0) {
            return node;
        }
    }
    return nullptr;
}

Node* Context::createNode(Node *parent, const char *name)
{
    if (!m_scene) { return nullptr; }

    auto node = FbxNode::Create(m_scene, name);
    if (!node) { return nullptr; }

    reinterpret_cast<FbxNode*>(parent ? parent : getRootNode())->AddChild(node);

    return node;
}

void Context::setTRS(Node *node_, float3 t, quatf r, float3 s)
{
    if (!node_) { return; }

    t *= m_opt.scale_factor;
    if (m_opt.flip_handedness) {
        t = swap_handedness(t);
        r = swap_handedness(r);
    }

    auto node = (FbxNode*)node_;
    node->LclTranslation.Set(ToP3(t));
    node->RotationOrder.Set(FbxEuler::eOrderZXY);
    node->LclRotation.Set(ToP3(to_eularZXY(r) * Rad2Deg));
    node->LclScaling.Set(ToP3(s));
}


void Context::addMesh(Node *node_, int num_vertices,
    const float3 points[], const float3 normals[], const float4 tangents[], const float2 uv[], const float4 colors[])
{
    if (!node_) { return; }
    if (!points) { return; } // points must not be null

    auto node = reinterpret_cast<FbxNode*>(node_);
    auto mesh = FbxMesh::Create(m_scene, "");

    {
        // set points
        mesh->InitControlPoints(num_vertices);
        auto dst = mesh->GetControlPoints();
        for (int i = 0; i < num_vertices; ++i) {
            dst[i] = ToP4(points[i] * m_opt.scale_factor);
        }
        if (m_opt.flip_handedness) { FlipHandedness(dst, num_vertices); }
    }

    if (normals) {
        // set normals
        auto element = mesh->CreateElementNormal();
        element->SetMappingMode(FbxGeometryElement::eByControlPoint);
        element->SetReferenceMode(FbxGeometryElement::eDirect);

        auto& da = element->GetDirectArray();
        da.Resize(num_vertices);
        auto dst = (FbxVector4*)da.GetLocked();
        for (int i = 0; i < num_vertices; ++i) {
            dst[i] = ToV4(normals[i]);
        }
        if (m_opt.flip_handedness) { FlipHandedness(dst, num_vertices); }
        da.Release((void**)&dst);
    }
    if (tangents) {
        // set tangents
        auto element = mesh->CreateElementTangent();
        element->SetMappingMode(FbxGeometryElement::eByControlPoint);
        element->SetReferenceMode(FbxGeometryElement::eDirect);

        auto& da = element->GetDirectArray();
        da.Resize(num_vertices);
        auto dst = (FbxVector4*)da.GetLocked();
        for (int i = 0; i < num_vertices; ++i) {
            dst[i] = ToV4(tangents[i]);
        }
        if (m_opt.flip_handedness) { FlipHandedness(dst, num_vertices); }
        da.Release((void**)&dst);
    }
    if (uv) {
        // set uv
        auto element = mesh->CreateElementUV("UVSet1");
        element->SetMappingMode(FbxGeometryElement::eByControlPoint);
        element->SetReferenceMode(FbxGeometryElement::eDirect);

        auto& da = element->GetDirectArray();
        da.Resize(num_vertices);
        auto dst = (FbxVector2*)da.GetLocked();
        for (int i = 0; i < num_vertices; ++i) {
            dst[i] = ToV2(uv[i]);
        }
        da.Release((void**)&dst);
    }
    if (colors) {
        // set colors
        auto element = mesh->CreateElementVertexColor();
        element->SetMappingMode(FbxGeometryElement::eByControlPoint);
        element->SetReferenceMode(FbxGeometryElement::eDirect);

        auto& da = element->GetDirectArray();
        da.Resize(num_vertices);
        auto dst = (FbxColor*)da.GetLocked();
        for (int i = 0; i < num_vertices; ++i) {
            dst[i] = ToC4(colors[i]);
        }
        da.Release((void**)&dst);
    }

    node->SetNodeAttribute(mesh);
    node->SetShadingMode(FbxNode::eTextureShading);
}

void Context::addMeshSubmesh(Node *node_, Topology topology, int num_indices, const int indices[], int material)
{
    if (!node_) { return; }

    auto node = reinterpret_cast<FbxNode*>(node_);
    auto mesh = node->GetMesh();
    if (!mesh) { return; }

    int vertices_in_primitive = 1;
    switch (topology)
    {
    case Topology::Points:    vertices_in_primitive = 1; break;
    case Topology::Lines:     vertices_in_primitive = 2; break;
    case Topology::Triangles: vertices_in_primitive = 3; break;
    case Topology::Quads:     vertices_in_primitive = 4; break;
    default: break;
    }

    if (topology == Topology::Triangles && m_opt.quadify) {
        // quadify

        const float threshold = m_opt.quadify_threshold_angle;
        int num_triangles = num_indices / 3;
        RawVector<int> qindices;
        RawVector<int> qcounts;
        RawVector<bool> qmerged;
        qmerged.resize_zeroclear(num_triangles);
        auto base_vertices = mesh->GetControlPoints();

        for (int ti1 = 0; ti1 < num_triangles; ++ti1) {
            if (qmerged[ti1]) { continue; }
            auto *tri1 = indices + (ti1 * 3);
            int quad_result[4];

            int merge_ti = -1;
            float min_angle = 180.0f;
            const float3 normal1 = normalize(cross(
                ToFloat3(base_vertices[tri1[1]] - base_vertices[tri1[0]]),
                ToFloat3(base_vertices[tri1[2]] - base_vertices[tri1[0]])));

            for (int ti2 = ti1 + 1; ti2 < num_triangles; ++ti2) {
                if (qmerged[ti2])
                    continue;
                auto *tri2 = indices + (ti2 * 3);

                int quad[6];
                std::copy(tri1, tri1 + 3, quad);
                std::copy(tri2, tri2 + 3, quad + 3);
                std::sort(quad, quad + 6);
                auto it = std::unique(quad, quad + 6);
                if (it != quad + 4)
                    continue;

                float3 normal2 = normalize(cross(
                    ToFloat3(base_vertices[tri2[1]] - base_vertices[tri2[0]]),
                    ToFloat3(base_vertices[tri2[2]] - base_vertices[tri2[0]])));
                if (dot(normal1, normal2) < 0.0f)
                    continue;

                float3 qvertices[4];
                for (int i = 0; i < 4; ++i)
                    qvertices[i] = ToFloat3(base_vertices[quad[i]]);

                float3 center = float3::zero();
                for (auto& v : qvertices)
                    center += v;
                center *= 0.25f;

                float angles[4]{
                    0.0f,
                    angle_between2_signed(qvertices[0], qvertices[1], center, normal1),
                    angle_between2_signed(qvertices[0], qvertices[2], center, normal1),
                    angle_between2_signed(qvertices[0], qvertices[3], center, normal1),
                };

                int cwi[4], quad_tmp[4];
                std::iota(cwi, cwi + 4, 0);
                std::sort(cwi, cwi + 4, [&angles](int a, int b) {
                    return angles[a] < angles[b];
                });
                for (int i = 0; i < 4; ++i) {
                    quad_tmp[i] = quad[cwi[i]];
                    qvertices[i] = ToFloat3(base_vertices[quad_tmp[i]]);
                }

                int corners[4][3]{
                    { 3, 0, 1 },
                    { 0, 1, 2 },
                    { 1, 2, 3 },
                    { 2, 3, 0 }
                };
                float diff = 0.0f;
                for (int i = 0; i < 4; ++i) {
                    float angle = angle_between2(
                        qvertices[corners[i][0]],
                        qvertices[corners[i][2]],
                        qvertices[corners[i][1]]) * Rad2Deg;
                    diff = std::max(diff, abs(angle - 90.0f));
                }
                if (diff < min_angle) {
                    merge_ti = ti2;
                    min_angle = diff;
                    std::copy(quad_tmp, quad_tmp + 4, quad_result);
                }
            }

            if (merge_ti != -1 && min_angle < threshold) {
                qmerged[merge_ti] = true;
                qindices.insert(qindices.end(), quad_result, quad_result + 4);
                qcounts.push_back(4);
            }
            else {
                qindices.insert(qindices.end(), tri1, tri1 + 3);
                qcounts.push_back(3);
            }
        }

        int pi = 0;
        int num_faces = (int)qcounts.size();
        if (m_opt.flip_faces) {
            for (int fi = 0; fi < num_faces; ++fi) {
                int count = qcounts[fi];
                mesh->BeginPolygon(material);
                for (int vi = count - 1; vi >= 0; --vi) {
                    mesh->AddPolygon(qindices[pi + vi]);
                }
                pi += count;
                mesh->EndPolygon();
            }
        }
        else {
            for (int fi = 0; fi < num_faces; ++fi) {
                int count = qcounts[fi];
                mesh->BeginPolygon(material);
                for (int vi = 0; vi < count; ++vi) {
                    mesh->AddPolygon(qindices[pi + vi]);
                }
                pi += count;
                mesh->EndPolygon();
            }
        }
    }
    else {
        int pi = 0;
        if (m_opt.flip_faces) {
            while (pi < num_indices) {
                mesh->BeginPolygon(material);
                for (int vi = vertices_in_primitive - 1; vi >= 0; --vi) {
                    mesh->AddPolygon(indices[pi + vi]);
                }
                pi += vertices_in_primitive;
                mesh->EndPolygon();
            }
        }
        else {
            while (pi < num_indices) {
                mesh->BeginPolygon(material);
                for (int vi = 0; vi < vertices_in_primitive; ++vi) {
                    mesh->AddPolygon(indices[pi + vi]);
                }
                pi += vertices_in_primitive;
                mesh->EndPolygon();
            }
        }
    }
}

void Context::addMeshSkin(Node *node_, Weights4 weights[], int num_bones, Node *bones[], float4x4 bindposes[])
{
    if (!node_) { return; }

    auto node = reinterpret_cast<FbxNode*>(node_);
    auto mesh = node->GetMesh();
    if (!mesh) { return; }

    auto skin = FbxSkin::Create(m_scene, "");
    mesh->AddDeformer(skin);

    RawVector<int> dindices;
    RawVector<double> dweights;
    int num_vertices = mesh->GetControlPointsCount();
    for (int bi = 0; bi < num_bones; ++bi) {
        if (!bones[bi]) { continue; }

        auto bone = reinterpret_cast<FbxNode*>(bones[bi]);
        auto cluster = FbxCluster::Create(m_scene, "");
        cluster->SetLink(bone);
        cluster->SetLinkMode(FbxCluster::eNormalize);

        auto bindpose = bindposes[bi];
        (float3&)bindpose[3] *= m_opt.scale_factor;
        if (m_opt.flip_handedness) {
            bindpose = swap_handedness(bindpose);
        }
        cluster->SetTransformMatrix(ToAM44(bindpose));

        GetInfluence(weights, num_vertices, bi, dindices, dweights);
        cluster->SetControlPointIWCount((int)dindices.size());
        dindices.copy_to(cluster->GetControlPointIndices());
        dweights.copy_to(cluster->GetControlPointWeights());

        skin->AddCluster(cluster);
    }
}

void Context::addMeshBlendShape(Node *node_, const char *name, float weight,
    const float3 delta_points[], const float3 delta_normals[], const float3 delta_tangents[])
{
    if (!node_) { return; }

    auto node = reinterpret_cast<FbxNode*>(node_);
    auto mesh = node->GetMesh();
    if (!mesh) { return; }

    // find or create blendshape deformer
    auto *blendshape = (FbxBlendShape*)mesh->GetDeformer(0, FbxDeformer::EDeformerType::eBlendShape);
    if (!blendshape) {
        blendshape = FbxBlendShape::Create(m_scene, "");
        mesh->AddDeformer(blendshape);
    }

    // find or create blendshape channel
    FbxBlendShapeChannel *channel = nullptr;
    {
        int num_channels = blendshape->GetBlendShapeChannelCount();
        for (int i = 0; i < num_channels; ++i) {
            auto c = blendshape->GetBlendShapeChannel(i);
            auto cname = c->GetName();
            if (strcmp(cname, name) == 0) {
                channel = c;
                break;
            }
        }
    }
    if (!channel) {
        channel = FbxBlendShapeChannel::Create(m_scene, name);
        blendshape->AddBlendShapeChannel(channel);
    }

    // create and add shape
    auto *shape = FbxShape::Create(m_scene, "");
    channel->AddTargetShape(shape, weight);

    int num_vertices = mesh->GetControlPointsCount();
    {
        // set points
        shape->InitControlPoints(num_vertices);
        auto base = mesh->GetControlPoints();
        auto dst = shape->GetControlPoints();
        if (delta_points) {
            for (int vi = 0; vi < num_vertices; ++vi) {
                float3 delta = delta_points[vi] * m_opt.scale_factor;
                if (m_opt.flip_handedness) { delta = swap_handedness(delta); }
                dst[vi] = ToP4(ToFloat3(base[vi]) + delta);
            }
        }
        else {
            for (int vi = 0; vi < num_vertices; ++vi) {
                dst[vi] = base[vi];
            }
        }
    }
    if (mesh->GetElementNormalCount() > 0) {
        // set normals
        auto element = shape->CreateElementNormal();
        element->SetMappingMode(FbxGeometryElement::eByControlPoint);
        element->SetReferenceMode(FbxGeometryElement::eDirect);
        auto& src_da = mesh->GetElementNormal()->GetDirectArray();
        auto& dst_da = element->GetDirectArray();
        dst_da.Resize(num_vertices);

        auto base = (const FbxVector4*)src_da.GetLocked(FbxLayerElementArray::eReadLock);
        auto dst = (FbxVector4*)dst_da.GetLocked();
        if (delta_normals) {
            for (int vi = 0; vi < num_vertices; ++vi) {
                float3 delta = delta_normals[vi];
                if (m_opt.flip_handedness) { delta = swap_handedness(delta); }

                auto& td = base[vi];
                auto t = ToFloat3(td);
                t = normalize(t + delta);
                dst[vi] = { t[0], t[1], t[2], td[3] };
            }
        }
        else {
            for (int vi = 0; vi < num_vertices; ++vi) {
                dst[vi] = base[vi];
            }
        }
        dst_da.Release((void**)&dst);
        src_da.Release((void**)&dst);
    }
    if (mesh->GetElementTangentCount() > 0) {
        // set tangents
        auto element = shape->CreateElementTangent();
        element->SetMappingMode(FbxGeometryElement::eByControlPoint);
        element->SetReferenceMode(FbxGeometryElement::eDirect);
        auto& src_da = mesh->GetElementTangent()->GetDirectArray();
        auto& dst_da = element->GetDirectArray();
        dst_da.Resize(num_vertices);

        auto base = (const FbxVector4*)src_da.GetLocked(FbxLayerElementArray::eReadLock);
        auto dst = (FbxVector4*)dst_da.GetLocked();
        if (delta_tangents) {
            for (int vi = 0; vi < num_vertices; ++vi) {
                float3 delta = delta_tangents[vi];
                if (m_opt.flip_handedness) { delta = swap_handedness(delta); }

                auto& td = base[vi];
                auto t = ToFloat3(td);
                t = normalize(t + delta);
                dst[vi] = { t[0], t[1], t[2], td[3] };
            }
        }
        else {
            for (int vi = 0; vi < num_vertices; ++vi) {
                dst[vi] = base[vi];
            }
        }
        dst_da.Release((void**)&dst);
        src_da.Release((void**)&dst);
    }
}
} // namespace fbxe
