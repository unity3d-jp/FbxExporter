using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace UTJ.FbxExporter
{
    public partial class FbxExporter
    {
        public struct Context
        {
            public IntPtr ptr;
            public static Context Null { get { Context r; r.ptr = IntPtr.Zero; return r;  } }
            public static implicit operator bool(Context v) { return v.ptr != IntPtr.Zero; }
        }
        public struct Node
        {
            public IntPtr ptr;
            public static Node Null { get { Node r; r.ptr = IntPtr.Zero; return r; } }
            public static implicit operator bool(Node v) { return v.ptr != IntPtr.Zero; }
        }

        public enum Format
        {
            FbxBinary,
            FbxAscii,
            FbxEncrypted,
            Obj,
        };

        public enum SystemUnit
        {
            Millimeter,
            Centimeter,
            Decimeter,
            Meter,
            Kilometer,
        };

        public enum Topology
        {
            Points,
            Lines,
            Triangles,
            Quads,
        };

        public struct ExportOptions
        {
            public bool flip_handedness;
            public bool flip_faces;
            public bool quadify;
            public bool quadify_full_search;
            public float quadify_threshold_angle;
            public float scale_factor;
            public SystemUnit system_unit;
            public bool transform;

            public static ExportOptions defaultValue
            {
                get
                {
                    return new ExportOptions {
                        flip_handedness = true,
                        flip_faces = true,
                        quadify = true,
                        quadify_full_search = false,
                        quadify_threshold_angle = 20.0f,
                        scale_factor = 1.0f,
                        system_unit = SystemUnit.Meter,
                        transform = true,
                    };
                }
            }
        };



        [DllImport("FbxExporterCore")] static extern Context fbxeCreateContext(ref ExportOptions opt);
        [DllImport("FbxExporterCore")] static extern void fbxeReleaseContext(Context ctx);

        [DllImport("FbxExporterCore")] static extern bool fbxeCreateScene(Context ctx, string name);
        [DllImport("FbxExporterCore")] static extern bool fbxeWriteAsync(Context ctx, string path, Format format);
        [DllImport("FbxExporterCore")] static extern bool fbxeIsFinished(Context ctx);

        [DllImport("FbxExporterCore")] static extern Node fbxeGetRootNode(Context ctx);
        [DllImport("FbxExporterCore")] static extern Node fbxeFindNodeByName(Context ctx, string name);

        [DllImport("FbxExporterCore")] static extern Node fbxeCreateNode(Context ctx, Node parent, string name);
        [DllImport("FbxExporterCore")] static extern void fbxeSetTRS(Context ctx, Node node, Vector3 t, Quaternion r, Vector3 s);
        [DllImport("FbxExporterCore")] static extern void fbxeAddMesh(Context ctx, Node node,
            int num_vertices, IntPtr points, IntPtr normals, IntPtr tangents, IntPtr uv, IntPtr colors);
        [DllImport("FbxExporterCore")] static extern void fbxeAddMeshSubmesh(Context ctx, Node node,
            Topology topology, int num_indices, IntPtr indices, int material);
        [DllImport("FbxExporterCore")] static extern void fbxeAddMeshSkin(Context ctx, Node node,
            IntPtr weights, int num_bones, IntPtr bones, IntPtr bindposes);
        [DllImport("FbxExporterCore")] static extern void fbxeAddMeshBlendShape(Context ctx, Node node,
            string name, float weight, IntPtr deltaPoints, IntPtr deltaNormals, IntPtr deltaTangents);

        [DllImport("FbxExporterCore")] static extern void fbxeGenerateTerrainMesh(
            float[,] heightmap, int width, int height, Vector3 size,
            IntPtr dst_vertices, IntPtr dst_normals, IntPtr dst_uv, IntPtr dst_indices);

    }

}
