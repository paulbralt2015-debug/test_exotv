# ExoTV Modern

Réécriture moderne et indépendante de l'interface ExoTV 4.55, ciblant Windows 10/11 x64.

## Fonctions disponibles dans cette version

- Lecteur vidéo intégré basé sur FFmpeg (libavformat/libavcodec/libswscale).
- Décodage audio FFmpeg et sortie par le périphérique audio Windows par défaut via Qt Multimedia.
- Lecture de fichiers MP4, MKV, AVI, MOV, MPEG/TS, WMV, FLV et WebM selon les codecs disponibles.
- Contrôles de lecture, barre de position, plein écran et formats source/4:3/16:9/étiré.
- Capture d'image BMP.
- Gel de la dernière image (Freeze).
- Masquage réglable des lignes supérieures (0 à 32).
- Décalage vertical réglable.
- Réglages de luminosité, contraste et saturation.
- Inversion des couleurs.
- Workflow GitHub Actions pour compilation Windows x64 et création d'un ZIP.

## Portage depuis ExoTV 4.55

L'archive originale exotv455.rar contient l'exécutable ExoTV 4.55, les pilotes et bibliothèques historiques, la documentation HTML et certains codes sources annexes (notamment NoTV et une sortie audio ASIO). Elle ne contient pas le code source du programme ExoTV lui-même.

Les commandes de gel d'image, masquage des lignes, décalage vertical et réglages d'image sont réimplémentées ici comme fonctions générales de traitement vidéo. Les fonctions spécifiques de décodage de systèmes d'accès conditionnel (Nagravision/Syster et Videocrypt) ne sont pas portées. Les entrées de menu correspondantes restent désactivées ; aucun ancien pilote VXD/SYS ou exécutable de l'archive n'est chargé.

PAL/SECAM des fichiers vidéo sont interprétés par FFmpeg à partir du flux et de ses métadonnées lorsque disponibles. Ce lecteur ne remplace pas un tuner ni une carte d'acquisition analogique.

## Compilation

La compilation cible Windows 10/11 x64 avec Qt 6.8.3 MSVC 2022 et vcpkg via GitHub Actions. Aucun générateur Ninja n'est utilisé.

