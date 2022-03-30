# Compiling

## Generating data-includes from binary assets

```sh
xxd -i GLOBDATA.BIN > GLOBDATA.BIN.inc
xxd -i MAP.BIN > MAP.BIN.inc
```

## Compiling using Emscripten

```sh
em++ main.cpp -o wasm-emscripten-dune-globe.html
```

The html file cannot be viewed as a local file in a browser, it needs to be 
served from a server (local or otherwise.)
