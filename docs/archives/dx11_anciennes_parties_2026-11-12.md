# DirectX 11 (wwe12_d3d11.exe) : état et verdict du point de décision : anciennes parties (archivées le 2026-11-12)

Parties finies de `docs/dx11.md`, recopiées telles quelles. Le verdict et l'état actuel sont dans le fichier principal.

## Ancien point de décision (fin de l'étape E3)

Menus justes, mais en match la scène 3D était presque noire et le jeu tournait à 30 images/s
(~31 ms de travail par image). C'est corrigé en E4 (voir ci-dessus).

## Pistes pour E4

1. **Scène noire.** Kaiko a vu la même chose sous WARP (« post-traitement noir »). Pistes, de la plus
   probable à la moins probable :
   - les copies de tuiles (`ResolveRegion` / `copyTextureRegion`) vers des textures à plusieurs
     niveaux ;
   - le format R11G11B10 ou RGBA16F de la scène ;
   - un swizzle mal écrit pour une texture « nulle ».

   Méthode : la couche de débogage D3D11 (`D3D11_CREATE_DEVICE_DEBUG`) et une capture RenderDoc D3D11.
2. **30 images/s.** Ce sont ~31 ms de travail par image. À profiler. Pistes :
   - le fil de relecture de plume D3D11 et le verrou du contexte ;
   - les 96 vérifications d'emplacements par dessin dans `D3D11BindSlots` ;
   - les copies de constantes.
3. **Reflets du soleil** : il n'y a pas encore d'UAV dans le backend, donc le comptage d'occlusion
   est coupé en DX11.
