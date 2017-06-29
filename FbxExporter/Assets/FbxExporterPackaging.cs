#if UNITY_EDITOR
using UnityEngine;
using UnityEditor;


public class FbxExporterPackaging
{
    [MenuItem("Assets/Make FbxExporter.unitypackage")]
    public static void MakePackage()
    {
        string[] files = new string[]
        {
            "Assets/UTJ/FbxExporter",
        };
        AssetDatabase.ExportPackage(files, "FbxExporter.unitypackage", ExportPackageOptions.Recurse);
    }

}
#endif // UNITY_EDITOR
