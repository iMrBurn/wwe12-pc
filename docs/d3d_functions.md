# Fonctions Direct3D (XDK) de WWE '12

Le rendu natif (repris des portages SvR 2008-2010 de nefariousjosiah, GPL-3.0) observe les
appels Direct3D du jeu par des crochets posés sur les fonctions de la bibliothèque D3D de la
XDK, liée statiquement dans `default.xex`. Il faut donc retrouver leur adresse dans WWE '12 :
c'est l'équivalent de `src/native/svr_d3d_names.h` (SvR 2010).

La bibliothèque D3D de WWE '12 occupe environ `0x82AA0000`–`0x82AC0000`
(SvR 2010 : vers `0x826D0000`).

## Méthode

1. Ancrages sûrs : les fonctions qui appellent des imports du noyau propres à l'affichage
   (`VdSwap`, `VdGetSystemCommandBuffer`, `VdPersistDisplay`…), dont le nom est conservé
   dans le code généré (`port/generated/default`).
2. Graphe d'appels autour de ces ancrages : SvR 2010 documente par exemple que `Present`
   appelle `Scissor`, `SetRenderTarget`, `SynchronizeToPresentationInterval`, `Swap` et
   `Resolve`.
3. Confirmation par le comportement : champs du device écrits, registres GPU touchés,
   appels observés en jeu.

## Outils

- `tools/callgraph.py` : appelants / appelés d'une fonction (appels `bl` directs).
- `tools/gen_d3d_census.py` + `-DWWE12_D3D_CENSUS=ON` : compte en jeu les appels à chaque
  fonction de la plage D3D, avec r3-r6 et la détection d'un conteneur de shader en r4/r5
  (`d3d_census.csv`, toutes les 2 s). Relevés dans `work/census/`.
- Recherche des en-têtes de paquets PM4 (`0xC0nnXX00`, opcode en bits 8-14) émis par
  chaque fonction : voir l'historique de session (à transformer en outil).

## Objet device

`0xADB97A80` : r3 constant de 242 fonctions de la plage (SvR 2011 : `0xAD977E80`).

## Types d'objets (champ `D3DResource::Common`)

Les objets D3D commencent par `0x0010000T`, le type `T` dans les bits bas, conforme aux
`D3DCOMMON_TYPE_*` de la XDK : 1 tampon de sommets, 2 tampon d'indices, 3 texture,
4 surface, 5 déclaration de sommets, 6 vertex shader, 7 pixel shader. Le recensement relève
le premier mot pointé par r4/r5/r6 : c'est ce qui identifie les fonctions `Set*`.

## Structure des objets (relevée en jeu, `--dump-words`, 2026-10-10)

WWE '12 ne passe pas par `Create*Shader` / `CreateVertexDeclaration` : le rendu natif doit
découvrir ces objets à leur premier `Set*`, en les lisant en mémoire.

- **Pixel shader** (type 7) : en-tête de 0x28 octets, puis **le conteneur `.xsc` en ligne**
  à `objet + 0x28` (`102A1100 …`). `+0x18` = adresse physique du microcode.
- **Vertex shader** (type 6) : `+0x20` = adresse physique du microcode ; le conteneur est à
  **`objet + 0x368`** (vu passer en r4 de `sub_82AB2D18` avec le même microcode en r5) ;
  l'objet contient aussi la table de lecture des sommets (`01010101…` dès `+0x34`).
- **Déclaration de sommets** (type 5) : pas de liste `D3DVERTEXELEMENT9` lisible
  (`+0x18` = 2, `+0x20` = `FF000000`) ; à reconstruire depuis les `vfetch` du vertex shader
  (format, offset, stride décrits par le microcode Xenos).

À vérifier sur plus d'objets : constance des offsets `0x28` / `0x368` (sinon chercher la
signature `0x102A11xx` après l'objet).

## Table

| Fonction XDK | WWE '12 | Certitude | Indice |
|---|---|---|---|
| `D3DDevice_Swap` (« Present » chez SvR 2011) | `sub_82AA8898` | **sûr** | seule fonction appelant `VdSwap` ; 30 appels/s (1 par image) ; r3 = device |
| `D3DDevice_Present` | (`sub_82AA9000` ?) | | appelle `Swap`, mais jamais appelée en jeu : le jeu appelle `Swap` directement |
| `D3DDevice_SynchronizeToPresentationInterval` | `sub_82AA83D8` | probable | `WAIT_REG_MEM`, 30 appels/s, juste avant `Swap` en mémoire, appelée par `Present` |
| `D3DDevice_DrawIndexedVertices` | `sub_82AA2E78` | probable | `DRAW_INDX`, ~21 000 appels/s, args (device, 6=TRIANGLESTRIP, 0, 0) |
| dessin non indexé (`DrawVertices` ?) | `sub_82AA29B8` | à vérifier | `DRAW_INDX`, ~1 100 appels/s |
| dessin « UP » (`DrawVerticesUP` / `DrawIndexedVerticesUP` ?) | `sub_82AA9118` | à vérifier | `DRAW_INDX_2` (données dans le paquet) |
| liaison des shaders au draw (`IM_LOAD`) | `sub_82AB2F08` | probable | seul émetteur d'`IM_LOAD` ; ~7 500 appels/s |
| mise en place d'un shader (`SetPixelShader`/`SetVertexShader` ?) | `sub_82AB2D18` | à vérifier | r4 pointe vers un conteneur `0x102A11xx` (200/200 appels) ; `LOAD_CONSTANT_CONTEXT` ; ~3 300 appels/s |
| création du device (`Direct3D_CreateDevice`) | `sub_82AAC300` | probable | seul émetteur de `ME_INIT` ; 1 appel |
| `D3DDevice_CreatePixelShader` / `CreateVertexShader` | ? | | aucune fonction de la plage ne reçoit de conteneur à la création : Yuke's semble charger ses shaders tout faits depuis `_startup.pac` (à confirmer) |
| `D3DDevice_SetTexture` | `sub_82A9F310` | probable | r4 = sampler 0-25, r5 → texture (type 3) ; ~19 000 appels/s |
| `D3DDevice_SetStreamSource` | `sub_82AA59E8` | probable | r4 = flux 0-15, r5 → tampon de sommets (type 1) |
| `D3DDevice_SetIndices` | `sub_82AA5B08` | probable | r4 → tampon d'indices (type 2) |
| `D3DDevice_SetRenderTarget` | `sub_82AA6290` | probable | r4 = 0-3, r5 → surface (type 4) ; `sub_82AA6F40` a la même signature (variante / Get ?) |
| `D3DDevice_SetDepthStencilSurface` | `sub_82AA6620` | probable | r4 → surface (type 4), un seul objet |
| `D3DDevice_SetVertexDeclaration` | `sub_82AA2268` | probable | r4 → déclaration (type 5) |
| `D3DDevice_SetVertexShader` | `sub_82AA2050` | probable | r4 → vertex shader (type 6) ; appelée exactement autant que la suivante |
| `D3DDevice_SetPixelShader` | `sub_82AA1E48` | probable | r4 → pixel shader (type 7) |
| (liaison VS + déclaration) | `sub_82AB2DC8` | à vérifier | r4 → VS (6), r5 → déclaration (5) |
| `D3DDevice_SetVertexShaderConstantF` | `sub_82AA1AF0` | à vérifier | r4 = registre 0-234, r5 → flottants, r6 = nombre de vecteurs |
| `D3DDevice_SetPixelShaderConstantF` | `sub_82AA1BC8` | à vérifier | r4 = registre 0-250, r5 → flottants, r6 = nombre |
| `D3DDevice_Resolve` | `sub_82AA1028` | probable | signature du crochet SvR `(dev, Flags, pSourceRect, pDestTexture, pDestPoint, DestLevel, DestSliceOrFace, pClearColor, …)` : r4 = 0, r5 = NULL, r6 → texture, r7-r9 = 0 ; 33 appels/image depuis le code du jeu (`82880DE0`) ; appelle `sub_82A9FED0` (setup interne) |
| `D3DDevice_Clear` | `sub_82AAA390` | probable | signature SvR `(dev, Count, pRects, Flags, Color, Z en f1, —, Stencil, EDRAMClear)` : r4 = 0, r5 = NULL, r6 = 0x3F, r7 = 0xFF000000 ; 17 appels/image depuis le code du jeu ; appelle la chaîne interne `82AAA258` / `82AA9BD8` / `82AA9638` (r4 = 0x3F) |
| `D3DDevice_BeginVertices` | `sub_82AA29B8` | **sûr** | (corrigé : ce n'est pas `DrawVertices`) chaque appelant l'appelle puis appelle `sub_82AA29A8` 0x14 octets plus loin ; args `(dev, PrimitiveType 13, VertexCount 4, VertexStride 20)` ; ~75 appels/image (quads d'interface, sprites, vidéo) |
| `D3DDevice_EndVertices` | `sub_82AA29A8` | **sûr** | 16 octets, juste avant `BeginVertices` (même disposition que SvR 2010) ; appelée autant de fois |
| `D3DDevice_DrawVertices` | ? | | pas encore identifiée |
| surfaces (viewport, scissor, Clear interne…) | `sub_82AA58A0`, `sub_82AA5EF8`, `sub_82AA6160`, `sub_82AA6200`, `sub_82AA4808` | à trier | r5 (ou r4+r5) → surface ; r5 = 0x60, r7 = 1.0f, r8 = 0xFFFFFFFF (Z + stencil) : sous-étapes de Clear / viewport |

### Signatures attendues par le rendu (crochets de SvR 2010)

Relevées dans `src/reblue/gpu/hooks/*.cpp` : elles fixent quels registres portent quoi.

- `Clear(dev, Count, pRects, Flags, Color, f64 Z, z_gpr_slot, Stencil, EDRAMClear)`
- `Resolve(dev, Flags, pSourceRect, pDestTexture, pDestPoint, DestLevel, DestSliceOrFace, pClearColor, …)`
- `DrawIndexedVertices(dev, PrimitiveType, BaseVertexIndex, StartIndex, IndexCount)`
- `BeginVertices(dev, PrimitiveType, VertexCount, VertexStride)` → pointeur (crochet « RESULT »)
- `SetStreamSource(dev, Stream, pVB, Offset, Stride)` ; `SetTexture(dev, Sampler, pTexture)` ;
  `SetIndices(dev, pIB)` ; `D3DIndexBuffer_Unlock(pIB)`

À identifier aussi (crochés par re:Blue) : `SetViewport`, `SetScissorRect`, `DrawVerticesUP`,
`BeginVertices` / `EndVertices`, `BeginTiling` / `EndTiling` (rendu en tuiles : le bug
« moitié d'écran » de l'entrée de CM Punk), `Create*` / `Lock` / `Unlock` des ressources,
`D3DResource_Release` / `AddRef`, `D3DQuery_Issue` / `GetData`.
| `D3DDevice_Resolve` | ? | | appelé par `Present` |
| `D3DDevice_Clear` | ? | | |
| `D3DDevice_SetRenderTarget` | ? | | appelé par `Present` |
| `D3DDevice_SetDepthStencilSurface` | ? | | |
| `D3DDevice_SetScissorRect` | ? | | appelé par `Present` |
| `D3DDevice_BeginVertices` / `EndVertices` | ? | | |
| `D3DDevice_DrawIndexedVertices` / `DrawVertices` | ? | | |
| `D3DVertexBuffer_Unlock` / `D3DIndexBuffer_Unlock` | ? | | |
| `D3DDevice_CreateVertexDeclaration` | ? | | |
| `D3DDevice_CreateVertexShader` / `CreatePixelShader` | ? | | reçoivent les conteneurs `.xsc` (signature `0x102A11xx`) |
| … | | | liste complète : `svr_d3d_names.h` de SvR 2010 |
