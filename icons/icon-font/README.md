# Projecteur Icon Font

All configurations for the `fontcustom` tool to build the projecteur-icons
font from the command line. See: https://github.com/FontCustom/fontcustom

The following files are necessary 
* `fontcustom.yml` - basic configuration
* `svg` - directory with svg icons (currently all from https://www.iconmonstr.com)
* `templates` - C++ header template

## Install `fontcustom`

For details see the github project of
`fontcustom`: https://github.com/FontCustom/fontcustom`

```
$ sudo apt-get install zlib1g-dev fontforge
$ git clone https://github.com/bramstein/sfnt2woff-zopfli.git sfnt2woff-zopfli && cd sfnt2woff-zopfli && make && mv sfnt2woff-zopfli /usr/local/bin/sfnt2woff
$ git clone --recursive https://github.com/google/woff2.git && cd woff2 && $ make clean all && sudo mv woff2_compress /usr/local/bin/ && sudo mv woff2_decompress /usr/local/bin/
$ gem install fontcustom
```

## Run font generation

Change to the directory containing the `fontcustom.yml` and the `svg` and
`templates` directory.

```
$ fontcustom compile
```

## Result output

The results will be in `./output/fonts`, including a C++ header with definition
for each glyph/icon and it's code point.
