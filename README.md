# Pokémon Emerald

This is a decompilation of Pokémon Emerald.

It builds the following ROM:

* [**pokeemerald.gba**](https://datomatic.no-intro.org/index.php?page=show_record&s=23&n=1961) `sha1: f3ae088181bf583e55daf962a92bb46f4f1d07b7`

To set up the repository, see [INSTALL.md](INSTALL.md).

## Docker build

To build the ROM without installing the toolchain on your host:

```bash
docker build -t pokeemerald-builder .
docker run --rm -v "$PWD:/workspace" pokeemerald-builder
```

The container installs agbcc into `tools/agbcc` on the first run, then runs `make`. The built ROM is written to `pokeemerald.gba`.

You can pass make arguments after the image name:

```bash
docker run --rm -v "$PWD:/workspace" pokeemerald-builder make -j"$(nproc)"
```

The same workflow is available with Docker Compose:

```bash
docker compose run --rm builder
```

For contacts and other pret projects, see [pret.github.io](https://pret.github.io/).
