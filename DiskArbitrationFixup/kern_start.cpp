/*
 * Copyright (c) 2019 John Davis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_file.hpp>

// DiskArbitrationAgent binary path.
static const char *binPathDiskArbitrationAgent = "/System/Library/Frameworks/DiskArbitration.framework/Versions/Current/Support/DiskArbitrationAgent";

static const uint32_t SectionActive = 1;

// Find:    83 F8 03 74 XX 83 F8 02
// Replace: 83 F8 03 74 XX 83 F8 0F
static uint8_t findBytes[] = { 0x83, 0xF8, 0x03, 0x74, 0xFF, 0x83, 0xF8, 0x02 };
static uint8_t replaceBytes[] = { 0x83, 0xF8, 0x03, 0x74, 0xFF, 0x83, 0xF8, 0x0F };

// Patching info for DiskArbitrationAgent binary.
static UserPatcher::BinaryModPatch patchBytesDiskArbitrationAgent {
    CPU_TYPE_X86_64,
    findBytes,
    replaceBytes,
    arrsize(findBytes),
    0,
    1,
    UserPatcher::FileSegment::SegmentTextText,
    SectionActive
};

// BinaryModInfo array containing all patches required.
static UserPatcher::BinaryModInfo binaryPatches[] {
    { binPathDiskArbitrationAgent, &patchBytesDiskArbitrationAgent, 1}
};

// DiskArbitrationAgent process info.
static UserPatcher::ProcInfo procInfo = { binPathDiskArbitrationAgent, static_cast<uint32_t>(strlen(binPathDiskArbitrationAgent)), 1 };

static void buildPatch(void *user, KernelPatcher &patcher) {
    DBGLOG("DiskArbitrationFixup", "buildPatches() start");
    
    // Get contents of binary.
    size_t outSize;
    uint8_t *buffer = FileIO::readFileToBuffer(binPathDiskArbitrationAgent, outSize);
    if (buffer == NULL) {
        DBGLOG("DiskArbitrationFixup", "Failed to read binary: %s\n", binPathDiskArbitrationAgent);
        procInfo.section = procInfo.SectionDisabled;
        return;
    }
    
    // Find where case 0x2 is located. This is where the dialog for unreadable disk would be shown.
    off_t index = 0;
    for (off_t i = 0; i < outSize; i++) {
        // Check if all bytes but byte 4 match.
        bool found = true;
        for (off_t b = 0; b < arrsize(findBytes); b++) {
            // Skip byte 4.
            if (b == 4)
                continue;
            
            // Check bytes. If mismatch, keep looking.
            if (buffer[i + b] != findBytes[b]) {
                found = false;
                break;
            }
        }
        
        // If we found a match, we are done.
        if (found) {
            index = i;
            break;
        }
    }
    
    // If we found no match, we can't go on.
    if (index == 0)
        panic("DiskArbitrationFixup: Failed to get index into binary: %s\n", binPathDiskArbitrationAgent);
    
    // Get byte 4.
    uint8_t *bufferOffset = buffer + index;
    findBytes[4] = replaceBytes[4] = bufferOffset[4];
    
    // Free buffer.
    Buffer::deleter(buffer);
}

// Main function.
static void dafxStart() {
    DBGLOG("DiskArbitrationFixup", "start");
    
    // Load callback.
    lilu.onPatcherLoadForce(buildPatch);
    
    // Load patches.
    lilu.onProcLoadForce(&procInfo, 1, nullptr, nullptr, binaryPatches, arrsize(binaryPatches));
}

// Boot args.
static const char *bootargOff[] {
    "-dafxoff"
};
static const char *bootargDebug[] {
    "-dafxdbg"
};
static const char *bootargBeta[] {
    "-dafxbeta"
};

// Plugin configuration.
PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::Mavericks,
    KernelVersion::Catalina,
    []() {
        dafxStart();
    }
};
