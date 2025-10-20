#!/usr/bin/env bash
set -e

PWD=$(dirname -- "$0")
cd "$PWD"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Building H5GG for all THEOS_PACKAGE_SCHEME types${NC}"
echo -e "${BLUE}========================================${NC}"

# Create build output directory
BUILD_DIR="$PWD/build_output"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Build for rootfull (traditional rooted jailbreak)
echo -e "\n${GREEN}[1/3] Building rootfull version (traditional rooted jailbreak)...${NC}"
make clean
make package FINALPACKAGE=1
if [ $? -eq 0 ]; then
    LATEST_DEB=$(ls -tr1 packages/*.deb 2>/dev/null | tail -1)
    if [ -n "$LATEST_DEB" ]; then
        cp "$LATEST_DEB" "$BUILD_DIR/H5GG-rootfull.deb"
        echo -e "${GREEN}✓ Rootfull build completed: $BUILD_DIR/H5GG-rootfull.deb${NC}"
    fi
else
    echo -e "${RED}✗ Rootfull build failed${NC}"
fi

# Build for rootless (/var/jb based jailbreaks like palera1n, Dopamine 2.0)
echo -e "\n${GREEN}[2/3] Building rootless version (/var/jb based)...${NC}"
make clean
THEOS_PACKAGE_SCHEME=rootless make package FINALPACKAGE=1
if [ $? -eq 0 ]; then
    LATEST_DEB=$(ls -tr1 packages/*.deb 2>/dev/null | tail -1)
    if [ -n "$LATEST_DEB" ]; then
        cp "$LATEST_DEB" "$BUILD_DIR/H5GG-rootless.deb"
        echo -e "${GREEN}✓ Rootless build completed: $BUILD_DIR/H5GG-rootless.deb${NC}"
    fi
else
    echo -e "${RED}✗ Rootless build failed${NC}"
fi

# Build for roothide (roothide jailbreak)
echo -e "\n${GREEN}[3/3] Building roothide version...${NC}"
make clean
THEOS_PACKAGE_SCHEME=roothide make package FINALPACKAGE=1
if [ $? -eq 0 ]; then
    LATEST_DEB=$(ls -tr1 packages/*.deb 2>/dev/null | tail -1)
    if [ -n "$LATEST_DEB" ]; then
        cp "$LATEST_DEB" "$BUILD_DIR/H5GG-roothide.deb"
        echo -e "${GREEN}✓ Roothide build completed: $BUILD_DIR/H5GG-roothide.deb${NC}"
    fi
else
    echo -e "${RED}✗ Roothide build failed${NC}"
fi

echo -e "\n${BLUE}========================================${NC}"
echo -e "${BLUE}Build Summary${NC}"
echo -e "${BLUE}========================================${NC}"
ls -lh "$BUILD_DIR"/*.deb 2>/dev/null || echo -e "${RED}No builds found in $BUILD_DIR${NC}"

echo -e "\n${GREEN}All builds completed!${NC}"
echo -e "${BLUE}Output directory: $BUILD_DIR${NC}"
