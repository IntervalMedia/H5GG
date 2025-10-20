#!/usr/bin/env bash
set -e

PWD=$(dirname -- "$0")
cd "$PWD"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}======================================================${NC}"
echo -e "${BLUE}  H5GG Universal Build System${NC}"
echo -e "${BLUE}  Building all modules for all jailbreak types${NC}"
echo -e "${BLUE}======================================================${NC}"

# Create master build output directory
MASTER_BUILD_DIR="$PWD/build_all_output"
rm -rf "$MASTER_BUILD_DIR"
mkdir -p "$MASTER_BUILD_DIR"

# Function to check if build succeeded
check_build() {
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Build successful${NC}"
        return 0
    else
        echo -e "${RED}✗ Build failed${NC}"
        return 1
    fi
}

# Build main H5GG tweak
echo -e "\n${YELLOW}========================================${NC}"
echo -e "${YELLOW}Building Main H5GG Tweak${NC}"
echo -e "${YELLOW}========================================${NC}"
./build-all-schemes.sh
check_build && cp -r build_output/* "$MASTER_BUILD_DIR/" 2>/dev/null || true

# Build globalview module
echo -e "\n${YELLOW}========================================${NC}"
echo -e "${YELLOW}Building GlobalView Module${NC}"
echo -e "${YELLOW}========================================${NC}"
cd globalview
./build-all-schemes.sh
check_build && cp -r build_output_globalview/* "$MASTER_BUILD_DIR/" 2>/dev/null || true
cd ..

# Build appstand module
echo -e "\n${YELLOW}========================================${NC}"
echo -e "${YELLOW}Building AppStand Module${NC}"
echo -e "${YELLOW}========================================${NC}"
cd appstand
./build-all-schemes.sh
check_build && cp -r build_output_appstand/* "$MASTER_BUILD_DIR/" 2>/dev/null || true
cd ..

echo -e "\n${BLUE}======================================================${NC}"
echo -e "${BLUE}  Build Complete!${NC}"
echo -e "${BLUE}======================================================${NC}"
echo -e "\n${GREEN}All packages built for:${NC}"
echo -e "  - ${GREEN}Rootfull${NC} (traditional rooted jailbreaks)"
echo -e "  - ${GREEN}Rootless${NC} (/var/jb based jailbreaks)"
echo -e "  - ${GREEN}Roothide${NC} (roothide jailbreak)"
echo -e "\n${YELLOW}Output directory: ${MASTER_BUILD_DIR}${NC}\n"

# Display all built packages
if ls "$MASTER_BUILD_DIR"/* 1> /dev/null 2>&1; then
    echo -e "${BLUE}Built packages:${NC}"
    ls -lh "$MASTER_BUILD_DIR"
else
    echo -e "${RED}No packages found in $MASTER_BUILD_DIR${NC}"
fi

echo -e "\n${GREEN}Installation instructions:${NC}"
echo -e "  1. Transfer the appropriate .deb file to your device"
echo -e "  2. Install with: ${YELLOW}dpkg -i <package-name>.deb${NC}"
echo -e "  3. Or use your package manager (Cydia, Sileo, Zebra, etc.)\n"
