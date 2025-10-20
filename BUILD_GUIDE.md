# Quick Start: Building H5GG with RootBridge

## One-Command Build (Recommended)

Build everything for all jailbreak types:
```bash
./build-all.sh
```

This creates packages for:
- ✅ Rootfull (checkra1n, unc0ver)
- ✅ Rootless (palera1n, Dopamine 2.0)  
- ✅ Roothide (roothide jailbreak)

All packages will be in `build_all_output/`

## Build Individual Modules

### Main H5GG Tweak
```bash
./build-all-schemes.sh
```
Output: `build_output/H5GG-*.deb`

### GlobalView (Floating App)
```bash
cd globalview
./build-all-schemes.sh
```
Output: `globalview/build_output_globalview/libH5GGApp-*.deb`

### AppStand (Standalone App)
```bash
cd appstand
./build-all-schemes.sh
```
Output: `appstand/build_output_appstand/h5ggapp-*.deb` + TrollStore versions

## Installation

1. **Identify your jailbreak type:**
   - Rootfull: checkra1n, unc0ver → use `*-rootfull.deb`
   - Rootless: palera1n, Dopamine 2.0 → use `*-rootless.deb`
   - Roothide: roothide jailbreak → use `*-roothide.deb`

2. **Transfer .deb to device:**
   ```bash
   scp build_all_output/*.deb mobile@<device-ip>:~/
   ```

3. **Install on device:**
   ```bash
   ssh mobile@<device-ip>
   dpkg -i ~/H5GG-rootless.deb  # or appropriate package
   ```

4. **Or use package manager:** Cydia, Sileo, Zebra, etc.

## Manual Build for Specific Scheme

If you only need one version:

```bash
# Rootfull only
make clean && make package FINALPACKAGE=1

# Rootless only
make clean && THEOS_PACKAGE_SCHEME=rootless make package FINALPACKAGE=1

# Roothide only
make clean && THEOS_PACKAGE_SCHEME=roothide make package FINALPACKAGE=1
```

## Need Help?

- Full documentation: [ROOTBRIDGE_INTEGRATION.md](ROOTBRIDGE_INTEGRATION.md)
- RootBridge details: [RootBridge/README.md](RootBridge/README.md)
- Discord: https://discord.gg/FAs4MH7HMc
- Forum: https://iosgods.com/forum/595-h5gg-igamegod/
