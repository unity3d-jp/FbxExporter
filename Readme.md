# FbxExporter
[English](https://translate.google.com/translate?sl=ja&tl=en&u=https://github.com/unity3d-jp/FbxExporter)  

*2018/04:
本プラグインで実装されている機能は全て [Unity Technologies 公式の FBX Exporter](https://www.assetstore.unity3d.com/en/#!/content/101408) に実装・統合されました。
よって、そちらの使用をおすすめします。*  
以下は古い内容です。


Unity の Mesh を .fbx 形式で書き出すプラグインです。  
Skinning や BlendShape もサポートしており、四角形化の機能も備えています。
[NormalPainter](https://github.com/unity3d-jp/NormalPainter) や [BlendShapeBuilder](https://github.com/unity3d-jp/BlendShapeBuilder) などで編集した Mesh を DCC ツールに戻す用途を想定しています。

Unity 2017.1 以上、Windows or Linux で動作を確認しています。  
Linux はソースからビルドする必要があります。(Plugin ディレクトリで cmake) Mac は fbx sdk のに絡む事情により現在非サポートです。

AssetStore に [Unity Technologies 公式の FBX Exporter](https://www.assetstore.unity3d.com/en/#!/content/101408) が別にありますが、そちらは Skinning や BlendShape がサポートされていません (2018/01 現在)。近い先にサポートされる見込みですが、それまでは本プラグインが役立つこともあるでしょう。

## 使い方
1. [FbxExporter.unitypackage](https://github.com/unity3d-jp/FbxExporter/releases/download/20180110/FbxExporter.unitypackage) をプロジェクトにインポート
2. Window -> Fbx Exporter でツールウィンドウを開く
3. エクスポートしたいオブジェクトを選択 (もしくは "Scope" を "Entire Scene" にして全オブジェクトを対象) にして "Export"

## License
[MIT](LICENSE.txt)