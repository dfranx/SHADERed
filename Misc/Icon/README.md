# Icon source file

The icon was designed using Inkscape. It most likely won't render correctly
in browsers due to the use of advanced blend modes. Please export it to a PNG
using Inkscape before displaying it somewhere, or use one of the PNGs found in
the `bin/` folder.

## Generating the Windows icon file

The Windows icon file can be generated using the following
[ImageMagick](https://imagemagick.org/) command:

```bash
magick convert bin/icon_256x256.png -define icon:auto-resize=256,128,64,48,32,16 icon.ico
```
