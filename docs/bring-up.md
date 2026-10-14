# DOA2 Ultimate bring-up

## Established baseline

The starter has public source, synthetic tests, custody checks and a pinned
lifter. Disc import, executable inventory and selected-target identity are
kept privately. There is no runnable game or validated function recipe yet.

Ultimate's launcher and main game are separate executables. Read the ignored
`private/target.json` to obtain the selected game path and its hashes; do not
assume the disc's default executable is the game.

The ISO hash establishes import consistency, not independent authentication
of a retail disc revision. Keep the selected revision provisional until its
identity has been checked against a trusted independent reference.

## Next steps

1. Confirm the selected executable is the DOA2 portion using its launcher
   transition and code, and authenticate the disc revision. Preserve that
   evidence and the executable identity under `private/`.
2. Inspect the target's section, stack and TLS ranges. Define safe host-owned
   kernel exports, heap and stack regions. The inherited DOA3 kernel block
   and fixed heap start overlap this image; its allocator was excluded.
3. Import the selected game into a separate private Ghidra project. Discover
   its own function bounds, indirect targets and ABI instead of importing
   sister-project address maps. Keep all exports under `private/`.
4. Use the pinned lifter's help and upstream interfaces to create a DOA2U
   generation recipe. Authenticate generated source with hashes/manifests.
   Fix translation defects in the lifter and regenerate.
5. Bring over the required kernel/runtime modules and write a minimal runner.
   Establish the natural startup frontier before adding library seams.
6. Rebind XAPI input/fibers and D3D8 5849 creation, clear and present. Then
   validate resources, shaders and draws through the semantic presenter.
7. Rebind DirectSound and CRI services. Verify movies, sound and worker
   scheduling, then test title/menu entry and the first natural fight.

Online libraries and dashboard/update binaries are present on the disc.
The initial scope is offline DOA2; any online behavior needs its own design.

Record each observed frontier privately. A public model-test pass or a
completed lift alone is not a gameplay result.
