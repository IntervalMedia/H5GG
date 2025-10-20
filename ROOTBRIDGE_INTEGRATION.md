# RootBridge Integration Summary

## What Was Implemented

This integration adds universal jailbreak support to H5GG by incorporating the RootBridge library, enabling the project to build for three different jailbreak types: rootfull, rootless, and roothide.

## Changes Made

### 1. RootBridge Library Integration

**Added Files:**
- `RootBridge/Headers/RootBridge.h` - Main header file
- `RootBridge/RootBridge.m` - Implementation file
- `RootBridge/vendor/apple/dyld_priv.h` - Apple private dyld headers
- `RootBridge/README.md` - Documentation for RootBridge usage

**RootBridge provides:**
- Runtime detection of jailbreak type (rootless vs rooted)
- Automatic path conversion for rootless jailbreaks
- Two main methods:
  - `[RootBridge isJBRootless]` - Detects if running on rootless jailbreak
  - `[RootBridge getJBPath:@"/path"]` - Converts paths to rootless format when needed

### 2. Build System Updates

**Main Makefile** (`Makefile`)
- Added `RootBridge/RootBridge.m` to H5GG_FILES
- Added `-IRootBridge/Headers` to H5GG_CFLAGS to include RootBridge headers

**Tweak.mm**
- Added `#import "RootBridge/Headers/RootBridge.h"` to import RootBridge functionality

### 3. Build Scripts

Created three build scripts to build all THEOS_PACKAGE_SCHEME variants:

**`build-all-schemes.sh`** - Builds main H5GG tweak
- Builds rootfull version (traditional rooted jailbreaks)
- Builds rootless version (/var/jb based, e.g., palera1n, Dopamine 2.0)
- Builds roothide version (roothide jailbreak)
- Outputs to `build_output/` directory

**`globalview/build-all-schemes.sh`** - Builds GlobalView module
- Same three variants for the GlobalView floating app module
- Outputs to `globalview/build_output_globalview/` directory

**`appstand/build-all-schemes.sh`** - Builds AppStand module
- Same three variants for the standalone app module
- Outputs to `appstand/build_output_appstand/` directory
- Also builds TrollStore versions (.tipa files)

**`build-all.sh`** - Master build script
- Builds all three modules for all three jailbreak types
- Collects all packages in `build_all_output/` directory
- Provides clear status output with color coding

### 4. Documentation Updates

**README.md**
- Added "Universal Jailbreak Support" section
- Documents the three supported jailbreak types
- Explains how to use the build scripts

**RootBridge/README.md**
- Comprehensive documentation about RootBridge
- Build instructions for all variants
- Manual build commands for specific schemes
- Explanation of how RootBridge works

**.gitignore**
- Added build output directories to prevent committing build artifacts:
  - `build_output/`
  - `build_output_globalview/`
  - `build_output_appstand/`
  - `build_all_output/`

## How It Works

### THEOS_PACKAGE_SCHEME

The THEOS build system uses the `THEOS_PACKAGE_SCHEME` environment variable to determine the target jailbreak type:

1. **Rootfull** (default/empty) - Traditional rooted jailbreaks
   - Uses standard root paths: `/Library`, `/usr/lib`, etc.
   - Example: checkra1n, unc0ver

2. **Rootless** (`THEOS_PACKAGE_SCHEME=rootless`) - Modern rootless jailbreaks
   - All jailbreak files are prefixed with `/var/jb`
   - Example paths: `/var/jb/Library`, `/var/jb/usr/lib`
   - Example jailbreaks: palera1n, Dopamine 2.0

3. **Roothide** (`THEOS_PACKAGE_SCHEME=roothide`) - Roothide jailbreak
   - Uses roothide-specific paths and implementation
   - Example: roothide jailbreak

### RootBridge Runtime Detection

RootBridge automatically detects the jailbreak type at runtime by:
1. Checking the path of the calling binary
2. If the binary is under `/Library` or `/usr`, it's rooted
3. Otherwise, it's rootless

This means a single binary can work on both rooted and rootless jailbreaks if paths are properly wrapped with `[RootBridge getJBPath:]`.

## Usage

### Building All Packages

```bash
# Build everything (all modules, all schemes)
./build-all.sh

# Build only main H5GG tweak
./build-all-schemes.sh

# Build only GlobalView
cd globalview && ./build-all-schemes.sh

# Build only AppStand
cd appstand && ./build-all-schemes.sh
```

### Manual Building for Specific Scheme

```bash
# Rootfull
make clean
make package FINALPACKAGE=1

# Rootless
make clean
THEOS_PACKAGE_SCHEME=rootless make package FINALPACKAGE=1

# Roothide
make clean
THEOS_PACKAGE_SCHEME=roothide make package FINALPACKAGE=1
```

### Installation

1. Transfer the appropriate .deb file to your iOS device
2. Install with: `dpkg -i <package-name>.deb`
3. Or use a package manager (Cydia, Sileo, Zebra, etc.)

Choose the package that matches your jailbreak:
- `*-rootfull.deb` for checkra1n, unc0ver
- `*-rootless.deb` for palera1n, Dopamine 2.0
- `*-roothide.deb` for roothide

## Benefits

1. **Universal Compatibility**: Single codebase works across all major jailbreak types
2. **Future-Proof**: As new jailbreak types emerge, RootBridge can be updated
3. **Easy Building**: Simple scripts to build all variants
4. **Clear Documentation**: Users know which package to install
5. **No Code Duplication**: Same source code for all variants

## Technical Details

### Files Modified
- `Makefile` - Added RootBridge compilation
- `Tweak.mm` - Added RootBridge import
- `.gitignore` - Added build output directories
- `README.md` - Added documentation

### Files Added
- `RootBridge/` directory with full library source
- `build-all-schemes.sh` - Main tweak build script
- `globalview/build-all-schemes.sh` - GlobalView build script
- `appstand/build-all-schemes.sh` - AppStand build script
- `build-all.sh` - Master build script
- `RootBridge/README.md` - RootBridge documentation

### Dependencies
- RootBridge requires Foundation framework (already a dependency)
- Uses Apple's private dyld headers (included in vendor/)
- No additional runtime dependencies

## Future Considerations

1. **Path Wrapping**: Currently RootBridge is integrated but paths in the code may need to be wrapped with `[RootBridge getJBPath:]` for full compatibility
2. **Plugin Support**: Plugins may also need RootBridge integration if they access jailbreak paths
3. **Testing**: Should be tested on actual devices with each jailbreak type
4. **CI/CD**: Could integrate build scripts into automated CI/CD pipeline

## References

- RootBridge Source: https://github.com/jjolano/RootBridge
- THEOS Documentation: https://theos.dev/
- Rootless Jailbreak Info: https://ios.cfw.guide/
