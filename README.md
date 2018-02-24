Auto deploy tool
=============================

auto redeploy when file updated or previous deployment died.

1. monitor a process, restart it when it's died.
2. watch a path, rerun command when it's updated.

## Quick start

1. build
    ```bash
    cmake -H. -Bbuild
    (cd build && make)
    ```
2. run
    ```bash
    ./build/autodeploy -c src/hi.sh -w src/
    ```

