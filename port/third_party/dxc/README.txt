DirectX Shader Compiler (DXC) v1.8.2407, Microsoft, release dxc_2024_07_31
https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.8.2407

Used by wwe12_d3d12.exe (Direct3D 12 version): dxcompiler.dll + dxil.dll are shipped next to the exe
(x64, identical to the official release files; checked 2026-10-26). dxil.dll is required: without it
the linked DXIL is unsigned and every pipeline fails.

Licence texts: LICENSE-MIT.txt and LICENSE-LLVM.txt (dxcompiler.dll), LICENSE-MS.txt (dxil.dll).
LICENSE-MS.txt (Microsoft) allows redistributing the distributable code with an application, but requires
(1) that end users accept terms protecting Microsoft at least as much and (2) that the distributor
indemnifies Microsoft for claims related to the distribution/use of the application. See that file.
