# Composants tiers

Le code propre au projet est sous licence **GPL-3.0** (voir le README). Les composants
ci-dessous gardent leur propre licence. Aucune donnée du jeu (XEX, fichiers du disque,
shaders extraits, code généré) n'est incluse dans ce dépôt ni dans ses versions publiées.

| Composant | Utilisation | Licence |
|---|---|---|
| [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) | recompilation statique, runtime | BSD 3 clauses (dérivé de [Xenia](https://xenia.jp)) |
| [XenosRecomp](https://github.com/hedge-dev/XenosRecomp) | conversion des shaders Xbox 360 en HLSL | MIT |
| [re:Blue](https://github.com/zolaware/reblue) | cœur du rendu natif (`port/src/reblue`) | BSD 3 clauses (`port/src/reblue/LICENSE.reblue`) |
| [plume](https://github.com/zolaware/plume) | couche Vulkan du rendu natif | MIT |
| [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler) | compilation HLSL → DXIL / SPIR-V | MIT (LLVM : Apache 2.0 avec exception LLVM) |

Modifications locales appliquées par `setup.ps1` :

- `port/patches/rexglue-sdk-wwe12.patch` : diagnostic DRED, marqueurs et historique des
  draws D3D12, option `d3d12_dred`.
- `port/patches/xenosrecomp-wwe12.patch` : constantes de spécialisation fixées à la
  compilation, interpolateurs de WWE '12, cube maps.

Projets dont ce portage s'inspire :

- [wwe-svr2011-pc](https://github.com/KaikoClanworth1/wwe-svr2011-pc) : méthode uniquement,
  aucun code repris (pas de licence publiée).
- Portages SvR 2008/2009/2010 de [nefariousjosiah](https://github.com/nefariousjosiah)
  (GPL-3.0) : leur rendu natif (src/native, adaptations de src/reblue) est la base du rendu
  natif de WWE '12.

La liste complète des bibliothèques incluses dans l'exe et les DLL, avec leurs licences, est
dans `port/dist/THIRD_PARTY_NOTICES.txt` (livré avec chaque version, textes dans `licenses/`).
