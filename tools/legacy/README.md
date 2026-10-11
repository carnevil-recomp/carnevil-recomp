# Legacy extractors

`split_raw_img.py` and `split_partition.py` are the project author's open-source rewrites of
daemon1's `carnevilraw.exe` / `carnevilpart.exe` (XenTaX forum). They were used for the first
manual extraction and are kept for reference and as an independent check:

```
python3 split_raw_img.py carnevil.raw -o parts          # TRAP partition table -> part0..part16
python3 split_partition.py parts/part0 -o extracted     # LFN32 filesystem -> files
```

The maintained tools are `tools/trap.py` and `tools/lfn32.py`, which read the partition table and
the filesystem's info sector (first FAT sector, root directory sector, cluster size) instead of
hard-coding them, and were verified to produce byte-identical output on both CarnEvil disks.
`./setup.sh --raw IMAGE` uses them.
