#include "pch.h"
#include "MeshUtils/MeshUtils.h"
#include "fbxeContext.h"
#include "fbxeUtils.h"

#ifdef _WIN32
    #pragma comment(lib, "libfbxsdk-md.lib")
#endif

namespace fbxe {


struct SubmeshData
{
    RawVector<int> indices;
    int material_id = 0;
};
using SubmeshDataPtr = std::shared_ptr<SubmeshData>;

struct SkinData
{
    RawVector<Weights4> weights;
    RawVector<Node*> bones;
    RawVector<float4x4> bindposes;
    FbxSkin *fbxskin = nullptr;
};
using SkinDataPtr = std::shared_ptr<SkinData>;

struct BlendShapeFrameData
{
    RawVector<float3> delta_points;
    RawVector<float3> delta_normals;
    RawVector<float3> delta_tangents;
    float weight = 100.0f;
    FbxShape *fbxshape = nullptr;
};
using BlendShapeFrameDataPtr = std::shared_ptr<BlendShapeFrameData>;

struct BlendShapeData
{
    std::string name;
    std::vector<BlendShapeFrameDataPtr> frames;
    FbxBlendShapeChannel *fbxchannel = nullptr;
};
using BlendShapeDataPtr = std::shared_ptr<BlendShapeData>;

struct MeshData
{
    RawVector<float3> points;
    RawVector<float3> normals;
    RawVector<float4> tangents;
    RawVector<float2> uv;
    RawVector<float4> colors;
    SkinDataPtr skin;
    std::vector<SubmeshDataPtr> submeshes;
    std::vector<BlendShapeDataPtr> blendshapes;
    FbxNode *fbxnode = nullptr;
    FbxMesh *fbxmesh = nullptr;
    FbxBlendShape *fbxblendshape = nullptr;

    std::vector<std::function<void()>> tasks;
};
using MeshDataPtr = std::shared_ptr<MeshData>;


class Context : public IContext
{
public:
    Context(const ExportOptions *opt);
    ~Context() override;
    void release() override;
    void clear() override;

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

    bool doWrite(const char *path, Format format);

private:
    ExportOptions m_opt;
    FbxManager *m_manager = nullptr;
    FbxScene *m_scene = nullptr;
    std::map<Node*, MeshDataPtr> m_mesh_data;
    std::future<void> m_task;
};
using ContextPtr = std::shared_ptr<Context>;


static std::vector<ContextPtr> g_contexts;

fbxeAPI IContext* CreateContext(const ExportOptions *opt)
{
    auto ret = new Context(opt);
    g_contexts.emplace_back(ret);
    return ret;
}




Context::Context(const ExportOptions *opt)
{
    if (opt) { m_opt = *opt; }
    m_manager = FbxManager::Create();
}

Context::~Context()
{
    clear();
    if (m_manager) {
        m_manager->Destroy();
        m_manager = nullptr;
    }
}

void Context::release()
{
    // don't delete here to keep proceed async tasks
}

void Context::clear()
{
    if (m_task.valid()) {
        m_task.wait();
    }
    if (m_scene) {
        m_scene->Destroy(true);
        m_scene = nullptr;
    }
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

bool Context::write(const char *path_, Format format)
{
    if (!m_scene) { return false; }

    std::string path = path_;
    m_task = std::async(std::launch::async, [this, path, format]() {
        for (auto& p : m_mesh_data) {
            for (auto& task : p.second->tasks) {
                task();
            }
        }
        m_mesh_data.clear();

        doWrite(path.c_str(), format);
    });
    return true;
}

bool Context::doWrite(const char *path, Format format)
{
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
    node->SetNodeAttribute(mesh);
    node->SetShadingMode(FbxNode::eTextureShading);

    auto ptr = new MeshData();
    auto& data = *ptr;
    m_mesh_data[node].reset(ptr);
    if (points) data.points.assign(points, points + num_vertices);
    if (normals) data.normals.assign(normals, normals + num_vertices);
    if (tangents) data.tangents.assign(tangents, tangents + num_vertices);
    if (uv) data.uv.assign(uv, uv + num_vertices);
    if (colors) data.colors.assign(colors, colors + num_vertices);
    data.fbxnode = node;
    data.fbxmesh = mesh;

    auto body = [this, &data, mesh, num_vertices]() {
        {
            // set points
            if (m_opt.flip_handedness) {
                for (auto& v : data.points) { v = swap_handedness(v); }
            }
            if (m_opt.scale_factor != 1.0f) {
                for (auto& v : data.points) { v *= m_opt.scale_factor; }
            }

            mesh->InitControlPoints(num_vertices);
            auto dst = mesh->GetControlPoints();
            for (int i = 0; i < num_vertices; ++i) {
                dst[i] = ToP4(data.points[i]);
            }
        }

        if (!data.normals.empty()) {
            if (m_opt.flip_handedness) {
                for (auto& v : data.normals) { v = swap_handedness(v); }
            }

            // set normals
            auto element = mesh->CreateElementNormal();
            element->SetMappingMode(FbxGeometryElement::eByControlPoint);
            element->SetReferenceMode(FbxGeometryElement::eDirect);

            auto& da = element->GetDirectArray();
            da.Resize(num_vertices);
            auto dst = (FbxVector4*)da.GetLocked();
            for (int i = 0; i < num_vertices; ++i) {
                dst[i] = ToV4(data.normals[i]);
            }
            da.Release((void**)&dst);
        }
        if (!data.tangents.empty()) {
            if (m_opt.flip_handedness) {
                for (auto& v : data.tangents) { v = swap_handedness(v); }
            }

            // set tangents
            auto element = mesh->CreateElementTangent();
            element->SetMappingMode(FbxGeometryElement::eByControlPoint);
            element->SetReferenceMode(FbxGeometryElement::eDirect);

            auto& da = element->GetDirectArray();
            da.Resize(num_vertices);
            auto dst = (FbxVector4*)da.GetLocked();
            for (int i = 0; i < num_vertices; ++i) {
                dst[i] = ToV4(data.tangents[i]);
            }
            da.Release((void**)&dst);
        }
        if (!data.uv.empty()) {
            // set uv
            auto element = mesh->CreateElementUV("UVSet1");
            element->SetMappingMode(FbxGeometryElement::eByControlPoint);
            element->SetReferenceMode(FbxGeometryElement::eDirect);

            auto& da = element->GetDirectArray();
            da.Resize(num_vertices);
            auto dst = (FbxVector2*)da.GetLocked();
            for (int i = 0; i < num_vertices; ++i) {
                dst[i] = ToV2(data.uv[i]);
            }
            da.Release((void**)&dst);
        }
        if (!data.colors.empty()) {
            // set colors
            auto element = mesh->CreateElementVertexColor();
            element->SetMappingMode(FbxGeometryElement::eByControlPoint);
            element->SetReferenceMode(FbxGeometryElement::eDirect);

            auto& da = element->GetDirectArray();
            da.Resize(num_vertices);
            auto dst = (FbxColor*)da.GetLocked();
            for (int i = 0; i < num_vertices; ++i) {
                dst[i] = ToC4(data.colors[i]);
            }
            da.Release((void**)&dst);
        }
    };
    data.tasks.push_back(body);
}

void Context::addMeshSubmesh(Node *node, Topology topology, int num_indices, const int indices[], int material)
{
    auto it = m_mesh_data.find(node);
    if (it == m_mesh_data.end() || !it->second) { return; }

    auto& data = *it->second;
    auto smptr = new SubmeshData();
    auto& sm = *smptr;
    data.submeshes.emplace_back(smptr);
    sm.material_id = material;
    sm.indices.assign(indices, indices + num_indices);

    auto body = [this, &data, &sm, topology, num_indices, material]() {
        auto mesh = data.fbxmesh;

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
            RawVector<int> qindices;
            RawVector<int> qcounts;
            QuadifyTriangles(data.points, sm.indices, m_opt.quadify_threshold_angle, qindices, qcounts);

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
                        mesh->AddPolygon(sm.indices[pi + vi]);
                    }
                    pi += vertices_in_primitive;
                    mesh->EndPolygon();
                }
            }
            else {
                while (pi < num_indices) {
                    mesh->BeginPolygon(material);
                    for (int vi = 0; vi < vertices_in_primitive; ++vi) {
                        mesh->AddPolygon(sm.indices[pi + vi]);
                    }
                    pi += vertices_in_primitive;
                    mesh->EndPolygon();
                }
            }
        }
    };
    data.tasks.push_back(body);
}

void Context::addMeshSkin(Node *node, Weights4 weights[], int num_bones, Node *bones[], float4x4 bindposes[])
{
    if (num_bones == 0) { return; }
    auto it = m_mesh_data.find(node);
    if (it == m_mesh_data.end() || !it->second) { return; }

    auto& data = *it->second;
    auto skinptr = new SkinData();
    auto& skin = *skinptr;
    data.skin.reset(skinptr);

    int num_vertices = (int)data.points.size();
    skin.weights.assign(weights, weights + num_vertices);
    skin.bones.assign(bones, bones + num_bones);
    skin.bindposes.assign(bindposes, bindposes + num_bones);

    auto body = [this, &data, &skin, num_bones, num_vertices]() {
        auto fbxskin = FbxSkin::Create(m_scene, "");
        data.fbxmesh->AddDeformer(fbxskin);
        skin.fbxskin = fbxskin;

        RawVector<int> dindices;
        RawVector<double> dweights;
        for (int bi = 0; bi < num_bones; ++bi) {
            if (!skin.bones[bi]) { continue; }

            auto bone = reinterpret_cast<FbxNode*>(skin.bones[bi]);
            auto cluster = FbxCluster::Create(m_scene, "");
            cluster->SetLink(bone);
            cluster->SetLinkMode(FbxCluster::eNormalize);

            auto bindpose = skin.bindposes[bi];
            (float3&)bindpose[3] *= m_opt.scale_factor;
            if (m_opt.flip_handedness) {
                bindpose = swap_handedness(bindpose);
            }
            cluster->SetTransformMatrix(ToAM44(bindpose));

            GetInfluence(skin.weights.data(), num_vertices, bi, dindices, dweights);
            cluster->SetControlPointIWCount((int)dindices.size());
            dindices.copy_to(cluster->GetControlPointIndices());
            dweights.copy_to(cluster->GetControlPointWeights());

            fbxskin->AddCluster(cluster);
        }
    };
    data.tasks.push_back(body);
}

void Context::addMeshBlendShape(Node *node, const char *name, float weight,
    const float3 delta_points[], const float3 delta_normals[], const float3 delta_tangents[])
{
    auto it = m_mesh_data.find(node);
    if (it == m_mesh_data.end() || !it->second) { return; }

    auto& data = *it->second;

    // find or create blendshape deformer
    if (!data.fbxblendshape) {
        data.fbxblendshape = FbxBlendShape::Create(m_scene, "");
        data.fbxmesh->AddDeformer(data.fbxblendshape);
    }

    // find or create blendshape channel
    BlendShapeData *blendshape = nullptr;
    for (auto& bs : data.blendshapes) {
        if (bs->name == name) {
            blendshape = bs.get();
            break;
        }
    }
    if (!blendshape) {
        blendshape = new BlendShapeData();
        data.blendshapes.emplace_back(blendshape);

        blendshape->name = name;
        blendshape->fbxchannel = FbxBlendShapeChannel::Create(m_scene, name);
        data.fbxblendshape->AddBlendShapeChannel(blendshape->fbxchannel);
    }

    // create and add shape
    auto *frameptr = new BlendShapeFrameData();
    auto& frame = *frameptr;
    frame.fbxshape = FbxShape::Create(m_scene, "");
    blendshape->fbxchannel->AddTargetShape(frame.fbxshape, weight);
    blendshape->frames.emplace_back(frameptr);

    int num_vertices = (int)data.points.size();
    if (delta_points) frame.delta_points.assign(delta_points, delta_points + num_vertices);
    if (delta_normals) frame.delta_normals.assign(delta_normals, delta_normals + num_vertices);
    if (delta_tangents) frame.delta_tangents.assign(delta_tangents, delta_tangents + num_vertices);
    frame.weight = weight;

    auto body = [this, &data, &frame, num_vertices]() {
        {
            // set points
            frame.fbxshape->InitControlPoints(num_vertices);
            auto dst = frame.fbxshape->GetControlPoints();
            auto base = data.points.data();
            if (!frame.delta_points.empty()) {
                for (int vi = 0; vi < num_vertices; ++vi) {
                    float3 delta = frame.delta_points[vi] * m_opt.scale_factor;
                    if (m_opt.flip_handedness) { delta = swap_handedness(delta); }
                    dst[vi] = ToP4(base[vi] + delta);
                }
            }
            else {
                for (int vi = 0; vi < num_vertices; ++vi) {
                    dst[vi] = ToP4(base[vi]);
                }
            }
        }
        {
            // set normals
            auto element = frame.fbxshape->CreateElementNormal();
            element->SetMappingMode(FbxGeometryElement::eByControlPoint);
            element->SetReferenceMode(FbxGeometryElement::eDirect);
            auto& dst_da = element->GetDirectArray();
            dst_da.Resize(num_vertices);

            auto base = data.normals.data();
            auto dst = (FbxVector4*)dst_da.GetLocked();
            if (!frame.delta_normals.empty()) {
                for (int vi = 0; vi < num_vertices; ++vi) {
                    float3 delta = frame.delta_normals[vi];
                    if (m_opt.flip_handedness) { delta = swap_handedness(delta); }
                    dst[vi] = ToV4(normalize(base[vi] + delta));
                }
            }
            else {
                for (int vi = 0; vi < num_vertices; ++vi) {
                    dst[vi] = ToV4(base[vi]);
                }
            }
            dst_da.Release((void**)&dst);
        }
        {
            // set tangents
            auto element = frame.fbxshape->CreateElementTangent();
            element->SetMappingMode(FbxGeometryElement::eByControlPoint);
            element->SetReferenceMode(FbxGeometryElement::eDirect);
            auto& dst_da = element->GetDirectArray();
            dst_da.Resize(num_vertices);

            auto dst = (FbxVector4*)dst_da.GetLocked();
            auto base = data.tangents.data();
            if (!frame.delta_tangents.empty()) {
                for (int vi = 0; vi < num_vertices; ++vi) {
                    float3 delta = frame.delta_tangents[vi];
                    if (m_opt.flip_handedness) { delta = swap_handedness(delta); }
                    float4 t = base[vi];
                    (float3&)t = normalize((float3&)t + delta);
                    dst[vi] = ToV4(t);
                }
            }
            else {
                for (int vi = 0; vi < num_vertices; ++vi) {
                    dst[vi] = ToV4(base[vi]);
                }
            }
            dst_da.Release((void**)&dst);
        }
    };
    data.tasks.push_back(body);
}
} // namespace fbxe
