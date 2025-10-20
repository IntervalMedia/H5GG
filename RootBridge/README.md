# RootBridge Integration

H5GG now includes [RootBridge](https://github.com/jjolano/RootBridge) to support universal builds that work across different jailbreak types.

## What is RootBridge?

RootBridge is an iOS developer framework that makes it possible to compile "universal" binaries that work on:
- **Rootfull** jailbreaks (traditional rooted, e.g., checkra1n, unc0ver)
- **Rootless** jailbreaks (using /var/jb prefix, e.g., palera1n, Dopamine 2.0)
- **Roothide** jailbreaks (roothide specific implementation)

## Building for All Jailbreak Types

### Build Everything (Recommended)

To build all H5GG modules for all three jailbreak types:

```bash
./build-all.sh
```

This will build:
- Main H5GG tweak
- GlobalView module
- AppStand module

All packages will be collected in `build_all_output/` directory.

### Main H5GG Tweak

To build only the main H5GG tweak for all three jailbreak types:

```bash
./build-all-schemes.sh
```

This will create three .deb packages in `build_output/`:
- `H5GG-rootfull.deb` - For traditional rooted jailbreaks
- `H5GG-rootless.deb` - For /var/jb based rootless jailbreaks
- `H5GG-roothide.deb` - For roothide jailbreak

### GlobalView Module

```bash
cd globalview
./build-all-schemes.sh
```

Builds will be in `globalview/build_output_globalview/`

### AppStand Module

```bash
cd appstand
./build-all-schemes.sh
```

Builds will be in `appstand/build_output_appstand/`

## Manual Building

You can also build for specific schemes manually:

### Rootfull (Traditional)
```bash
make clean
make package FINALPACKAGE=1
```

### Rootless (/var/jb based)
```bash
make clean
THEOS_PACKAGE_SCHEME=rootless make package FINALPACKAGE=1
```

### Roothide
```bash
make clean
THEOS_PACKAGE_SCHEME=roothide make package FINALPACKAGE=1
```

## How RootBridge Works

RootBridge provides two main methods:

- `[RootBridge isJBRootless]` - Detects if running on rootless jailbreak
- `[RootBridge getJBPath:@"/path"]` - Converts paths to rootless format when needed

The framework automatically detects the jailbreak type at runtime and adjusts paths accordingly. On rootless jailbreaks, paths like `/Library/MobileSubstrate` are automatically converted to `/var/jb/Library/MobileSubstrate`.

## Integration Details

RootBridge has been integrated into H5GG by:
1. Adding RootBridge source files to `RootBridge/` directory
2. Including RootBridge.m in compilation
3. Adding RootBridge headers to include path
4. Importing RootBridge in Tweak.mm

The integration ensures H5GG can run on any modern jailbreak without modification.
