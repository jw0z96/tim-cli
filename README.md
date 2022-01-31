# timpack

Packs texture data and palette data into the TIM file format, for use on the Sony Playstation.

## Usage

### Texture Preparation

This tool expects the texture data to be pre-quantised, and for the colours to map directly
to those within the first palette supplied. To generate these, use ImageMagick:

```bash
# Convert an arbritrary input image to 16 or 256 colours
convert input.png \
	-colors 16 \ # 16 for 4bpp, 256 for 8bpp
	output_quantised.png

# Extract the unique colour values to generate a palette
convert output_quantised.png \
	-unique-colors \
	output_palette.png
```

Experiment with ImageMagick's dithering options (`-dither Riemersma` & `-dither FloydSteinberg`) when generating the quantised image if necessary.

Please ensure that the generated palette image contains the correct number of colours for the selected format (16 for 4bpp, 256 for 8bpp), or a multiple of these if using multiple palettes. If the palette generated by ImageMagick contains fewer colours than requested (which may be valid if the input image already had fewer than the specified number of colours), amend it in your image editor of choice such that the image dimensions are those of a valid palette.

### Tool Usage

```bash
timpack --bpp=4 \ # 4 for 16 colour, 8 for 256 colour
	--texture=<texture file> \
	--palette=<palette file> \
	<output TIM file>
```
## TODO

- support semitransparent mode, currently STP is forced 1
- support 15 and 24 bit direct colour modes
- SDL viewer for debug?
