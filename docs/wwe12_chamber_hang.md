# Blocage en plein match (Elimination Chamber)

> **État au 2026-11-12 : CORRIGÉ** (2026-10-20, commits 7fc1d4e et b0ff90b, `port/src/native/wwe12_actor_jobs.cpp`).
> - Symptôme (v0.8.0-beta, trace d'un joueur sur 940MX) : le jeu se figeait en plein match, souvent en
>   Elimination Chamber.
> - Cause : l'ouvrier de jobs par catcheur (et le second, `sub_821E0AD0`) marquait un job « fini » **avant** de
>   le signaler. Le thread moteur maître (`sub_821E2B98` / `sub_82475210`) attendait alors pour toujours un
>   signal déjà perdu. C'était l'hypothèse n°1 de l'analyse.
> - Correctif : les deux ouvriers sont réécrits en C++ natif, avec le bon ordre (signaler, puis marquer).
> - Plus vu depuis. À rouvrir seulement si un joueur envoie un `hang_trace.txt` du même genre.

L'analyse complète du trace est archivée :
`docs/archives/wwe12_chamber_hang_anciennes_parties_2026-11-12.md`. On y trouve la table statique des threads
du moteur, le rôle de chaque thread, les chemins de signalisation, les outils `work\rdc\map_lookup.py` et
`xex_thread_table.py`, et les adresses corrigées. C'est utile pour lire un futur `hang_trace.txt`.
