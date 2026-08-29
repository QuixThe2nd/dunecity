# Dune2R Android Debug Release

Status: test build verified on Android 13 Ulefone Armor 21.

APK:

```text
GitHub Release asset: DuneLegacy.apk
```

Verified behavior:

```text
landscape display
single-finger tap and drag
Android Back as Escape
two-finger right-click gesture
```

The debug APK embeds the base staged payload under `assets/dune2r_payload/`.
Large Dune2R remastered atlases are downloaded independently from the in-game
`Dune2R EditoR` asset manager and are not part of the APK.

The Android launcher icon is staged from the existing desktop icon asset:
`dunecity-128x128.png`.
