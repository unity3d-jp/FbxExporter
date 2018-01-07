call buildtools.bat

msbuild FbxExporterCore.vcxproj /t:Build /p:Configuration=Master /p:Platform=x64 /m /nologo
