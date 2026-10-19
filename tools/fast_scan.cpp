#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

int main(int argc, char* argv[]) {
    const char* path = (argc > 1) ? argv[1] : "client.dll";
    std::ifstream f(path, std::ios::binary);
    if (!f) return 1;
    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(size);
    f.read((char*)buf.data(), size);

    uint64_t image_base = 0x180000000;
    // .text section is at file offset 0x400, RVA 0x1000, size 0x1AAAA00
    uint64_t text_rva = 0x1000;
    uint64_t text_off = 0x400;
    uint64_t text_size = 0x1AAAA00;

    std::vector<uint64_t> targets = {
        0x181CA25F4, // "cl: CreateMove - Frame %d..."
        0x181CA2344, // "cl: CreateMove - Invalid player history..."
        0x181C0FB93, // "sv_subtick_movement_view_angles"
        0x181C0F4C4  // "For subtick jumps, if this much time..."
    };

    for (uint64_t target_va : targets) {
        std::cout << "Searching for references to 0x" << std::hex << target_va << std::dec << "...\n";
        int64_t const_val = (int64_t)(target_va - (image_base + text_rva) - 4);
        const uint8_t* pText = buf.data() + text_off;
        for (int64_t k = 0; k < (int64_t)text_size - 4; ++k) {
            int32_t val32 = *(const int32_t*)(pText + k);
            if (k + val32 == const_val) {
                uint64_t instr_va = image_base + text_rva + k - 3; // usually 3-byte prefix (48 8d 15)
                std::cout << "  Found displacement at file offset 0x" << std::hex << (text_off + k)
                          << ", approx instruction VA: 0x" << (image_base + text_rva + k)
                          << " (disp=0x" << val32 << ")" << std::dec << "\n";
            }
        }
    }
    return 0;
}
