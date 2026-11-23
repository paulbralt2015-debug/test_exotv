# ExoTV Modern

Réécriture moderne et indépendante de l'interface ExoTV historique, ciblant Windows 10/11 x64.

## Objectif de cette première base
- Lecteur vidéo intégré basé sur FFmpeg (libavformat/libavcodec/libswscale).
- Décodage audio FFmpeg et sortie par le périphérique audio Windows par défaut via Qt Multimedia.
- Lecture de fichiers MP4, MKV, AVI, MOV, MPEG/TS, WMV, FLV, WebM selon les codecs FFmpeg disponibles.
- Contrôles de lecture, barre de position, plein écran et ratios source/4:3/16:9.
- Capture d'image BMP.
- Workflow GitHub Actions pour compilation Windows x64 et création d'un ZIP.

## Compilation locale
Installer Qt 6.8.3 MSVC 2022 et vcpkg, puis :

```powershell
cmake -S . -B build -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

## GitHub Actions
1. Créer un dépôt GitHub vide.
2. Ajouter tous les fichiers de ce projet à la racine.
3. Pousser sur `main` ou `master`.
4. Ouvrir l'onglet **Actions** et télécharger l'artifact `ExoTVModern-Windows-x64`.

## État réel du portage
Cette livraison est une première base compilable à valider sur le runner Windows. Elle ne prétend pas avoir déjà réimplémenté tous les algorithmes propriétaires/historiques d'ExoTV 4.55 : leur code source n'est pas présent dans l'archive fournie. Les entrées de menu historiques sont affichées mais désactivées tant que leurs algorithmes ne sont pas portés et testés sur des séquences de test autorisées. Les anciens pilotes VXD/SYS ne sont pas utilisés.

## Notes techniques
- Aucun pilote d'acquisition analogique n'est requis.
- Aucun Line-In n'est requis : l'audio sort sur le périphérique Windows par défaut.
- Les anciens pilotes et exécutables de l'archive ne sont pas chargés.
- Cette base nécessite une passe de compilation/correction sur GitHub Actions avant de considérer le binaire comme validé.
