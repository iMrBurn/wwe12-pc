# Optimisations du recomp WWE 2K14 (Jarrod) : ce qu'on peut reprendre : anciennes parties (archivées le 2026-11-12)

Parties finies de `docs/optimisation_jarrod.md`, recopiées telles quelles. 

## Sa méthode (pour la 1.1)

Ses notes ne parlent pas de décompilation en C++ lisible. Elles disent seulement :
- **Code natif à la main :** une fonction du jeu (la déformation des catcheurs) a été réécrite en
  natif.
- **Vérifications :** chaque changement est contrôlé image par image (« pixel-identique »,
  « read-backs octet pour octet », replay comparé image par image).
- **Mesures :** il fait un test de performance intégré (deux entrées et un début de match, deux
  fois) et demande aux joueurs leurs résultats sur portables.
- **Organisation :** il travaille par séances numérotées et garde un patch par séance.

Une idée à reprendre : un **test de performance intégré**, reproductible, qui donne aux joueurs des
chiffres comparables.

## Proposition

1. **Tout de suite (petit, sans risque) :** le point 1, la limite de read-back de 6 à 32 images. Il
   faut tester avec une tenue Superstar Threads en match.
2. **Pour les petits PC, en 1.0 :** les points 2, 3 et 4. Ils touchent l'attente du CPU, les
   pipelines et l'affichage des images en retard. On mesure avec `wwe12_dev_affinity=F` et le
   journal `[load]`.
3. **Plus tard (1.1) :** les points 6, 7, 8 et 10. Ils demandent de lire ses patchs (accord de
   l'utilisateur pour le téléchargement, et de Jarrod pour la reprise).
