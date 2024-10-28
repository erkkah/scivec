# SCIVEC - a bitmap to SCI0 PIC resource converter

SCIVEC is a tool for creating picture resources for SCI0 - based games.
The tool reads many bitmap image formats and converts them to the vector-based
format required for SCI0 games.

[More info about SCI games.](http://sci.sierrahelp.com/index.html)

## Running the tool

SCIVEC is a cross-platform command-line tool with two basic operation modes. It can *show* existing SCI0 picture resource files or *convert* bitmap files to SCI0 format.

To convert a file, simply run:

```shell
scivec convert myfile.png pic.123
```

To show a SCI0 picture file:

```shell
scivec show pic.123
```

Downloads are provided on the [releases](releases) page.

## Details

The tool loads images of any resolution, but will only use the upper-left 320x190 pixels as input. The input is not scaled.

Then, the image is mapped to the EGA palette.

The resulting image is then fed into the vectorizer, which starts by creating a palette of the image colors.

The SCI0 picture format has four 40-color palettes where each color is a pair of colors from the 16-color EGA palette. Entries with pairs of equal value will produce a solid color, while different-valued entries will produce a semi-dithered result by alternating between the two corresponding EGA colors.

With the palette in place, areas of the same color are identified and converted to a series of SCI lines, patterns and fills.

The resulting SCI picture resource is then loaded and rendered to make sure the result is a pixel-perfect representation of the original bitmap.

## Troubleshooting

If the tool fails to convert a bitmap, [please report a bug](issues).

If the tool reports "Parsed file not equal to original", you can add the `-noverify` flag to the `convert` command to let the tool continue anyway.

The `-show` flag can be added to view and debug the converted image.
