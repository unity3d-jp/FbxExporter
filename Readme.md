# FbxExporter

## Usage
1. Import this package: [FbxExporter.unitypackage](https://github.com/unity3d-jp/FbxExporter/releases/download/20180109/FbxExporter.unitypackage)
2. Open Window -> Fbx Exporter
3. Select objects to export (or change scope to "Entire Scene") and export

## Features & Limitations
- Supported platforms are Windows (64 bit)
  - Also confirmed to work on Linux, but you need to build plugin from source.
- The only feature currently supported is exporting meshes. Animations, textures or any others will not be exported.
  - MeshRenderer, SkinnedMeshRenderer and Terrain component will be exported as meshes. Skinning and blend shape are supported.

## License
[MIT](LICENSE.txt)