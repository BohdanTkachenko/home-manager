# Asahi / NixOS install handoff

Continue the in-progress NixOS install on Bohdan's **2021 16-inch MacBook Pro, M1 Pro** (MacBookPro18,1 / j316sap).

## Goal

Install **NixOS as the primary Linux OS using Asahi**, driven by Bohdan's existing private repo:

- GitHub: `BohdanTkachenko/nix-home`
- That repo is already a full NixOS + Home Manager flake.
- Do **not** replace it with a standalone throwaway config unless absolutely required for bootstrap.
- Preserve macOS and the Asahi boot plumbing.

## Current disk / Asahi state

Asahi installer v0.9.2 was run from macOS.

macOS APFS was shrunk to ~115.67 GB.

Current internal disk is `/dev/nvme0n1`.

Known partitions:

- p1: Apple/iBoot related
- p2: macOS APFS
- p3: Asahi stub macOS
- p4: Asahi-created EFI partition, label **EFI - NIXOS**
  - Asahi reported EFI PARTUUID: `eeadc6a5-a40b-49e2-9af7-6d63635db548`
  - mounted at `/mnt/boot`
- p5: Apple RecoveryOSContainer, ~5 GiB — **DO NOT TOUCH**
- p6: newly created Linux LUKS partition, ~349.9 GiB, GPT type 8309

`sgdisk -v /dev/nvme0n1` was clean after creating p6.

## Encryption / filesystem state

Already done:

```text
/dev/nvme0n1p6
  -> LUKS2, label nixos-crypt
  -> opened as /dev/mapper/cryptroot
  -> Btrfs, label nixos
```

Btrfs subvolumes already created:

- `@`
- `@home`
- `@nix`

Expected live mounts:

- `/mnt` -> cryptroot subvol=@, options include compress=zstd,noatime
- `/mnt/home` -> subvol=@home
- `/mnt/nix` -> subvol=@nix
- `/mnt/boot` -> `/dev/nvme0n1p4` vfat

Before doing anything, verify with:

```bash
findmnt /mnt /mnt/home /mnt/nix /mnt/boot
lsblk -f
cryptsetup status cryptroot
```

If the machine has rebooted since this handoff, reopen LUKS and remount the existing filesystems. **Do not recreate or format them.**

## Generated NixOS config

Already ran:

```bash
nixos-generate-config --root /mnt
```

So these exist:

- `/mnt/etc/nixos/hardware-configuration.nix`
- generated `configuration.nix` (not intended as the final source of truth)

The generated hardware config correctly detected:

- `aarch64-linux`
- `cryptroot`
- Btrfs root/home/nix subvolumes
- `/boot` on the Asahi EFI partition

Use the generated hardware config as ground truth for UUIDs and mount config.

## Asahi firmware

Verified present:

```text
/mnt/boot/vendorfw/firmware.cpio
```

The live installer also has Asahi support under:

```text
/etc/nixos/apple-silicon-support
```

For the final flake, use the current supported nix-community Asahi integration. Verify the current `nixos-apple-silicon` API before editing the repo. The final host must have the equivalent of:

- Asahi hardware support enabled
- systemd-boot
- `boot.loader.efi.canTouchEfiVariables = false`
- access to the extracted vendor firmware

For a pure flake setup, likely copy `vendorfw/firmware.cpio` into the MacBook host directory in the repo and point `hardware.asahi.peripheralFirmwareDirectory` at that directory, following the current upstream docs.

## Existing nix-home repo facts

I inspected the current `BohdanTkachenko/nix-home` flake from GitHub.

Relevant facts:

- `nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05"`
- Home Manager tracks `release-26.05`
- It already has:
  - `mkNixosSystem = system: machineModule: ...`
  - explicit support for `aarch64-linux` package/script/devShell outputs
- Existing `nixosConfigurations` include x86 hosts like `dan-idea`, `nyancat`, `workbench`, etc.
- User account is `dan`.

Important: the common `mkNixosSystem` module list currently includes many x86/PC-specific modules and defaults (AMD/Intel CPU/GPU modules, PC peripherals, disk module, lanzaboote, etc.). Do not blindly reuse all of those on Apple Silicon. Either:
1. refactor the builder so the Mac gets a clean common/base layer plus Mac-specific modules, or
2. override/disable every PC-specific default cleanly.

Prefer a maintainable refactor over a pile of `mkForce false` overrides.

The repo's `nix-common` input currently points at:

```text
git+ssh://git@forge.cyber.place:2222/nix-home/nix-common
```

So evaluating the private flake from this live installer requires access to Bohdan's forge SSH credentials. Do not replace this permanently just to get the install working. If bootstrap access is a problem, discuss/choose a temporary bootstrap route without weakening the actual repo.

## Network state

At the time of handoff the live NixOS installer had **no Internet connection**.

First action should be:

```bash
nmtui
```

Connect Wi-Fi, then verify DNS/network, e.g.:

```bash
ping -c1 1.1.1.1
ping -c1 cache.nixos.org
```

## What NOT to do

- Do not repartition the disk.
- Do not format p4, p5, or p6.
- Do not touch Apple Recovery (p5).
- Do not delete macOS or the Asahi stub.
- Do not run an automatic disk partitioner.
- Do not lose the existing LUKS/Btrfs layout.
- Do not install Fedora Asahi.
- Do not build a separate long-term config outside `nix-home`.

## Desired next steps

1. Bring Wi-Fi up.
2. Verify current mounts/LUKS state.
3. Obtain/access `BohdanTkachenko/nix-home`.
4. Inspect the live generated `hardware-configuration.nix`.
5. Add a clean Apple-Silicon NixOS host to that existing flake (probably name it `macbook` or another sensible host name).
6. Integrate current `nixos-apple-silicon` support.
7. Preserve/copy the Asahi vendor firmware into the flake in the supported way.
8. Evaluate/build the new host.
9. Install with `nixos-install --flake <repo>#<host>` (or equivalent) against the already-mounted `/mnt`.
10. Before rebooting, verify the EFI/systemd-boot entries exist on p4.
11. Reboot into the internal NixOS install, unlock LUKS, then continue normal repo-based configuration.

Be cautious around Apple Silicon boot/partition state, but otherwise take ownership of the remaining install rather than making Bohdan type long blocks manually.
