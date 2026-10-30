# M²CL branding assets

`m2cl.svg` is the source for the existing application icon. The platform icon
exports and the README header reuse that mark.

The README banners are self-contained SVG files with outlined lettering. They
contain no linked images, font files, scripts or network resources. Both
language READMEs use the same artwork, with light/dark selection through
`<picture>` and `prefers-color-scheme`. The banners are documentation assets
and are not included in the application's resource bundles.

To regenerate the banners and local PNG previews:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/Qt"
cmake --build build --target readme_banner_renderer --parallel
QT_QPA_PLATFORM=offscreen build/readme_banner_renderer assets/branding artifacts/readme-banner
```

The renderer prefers Avenir Next, followed by Inter, Segoe UI, Noto Sans and
DejaVu Sans. The committed banners were rendered with Avenir Next. Font choice
only affects regeneration; viewing the SVG does not require the font to be
installed. Keep the existing exports when a matching regeneration font is not
available.
