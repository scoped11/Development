// stomp_plan — usermode-only placement preview (no vehicle, no kernel R/W)
#include "../src/mapper/stomp.h"
#include <cstdio>
#include <fstream>
#include <vector>

static std::vector<uint8_t> ReadBin(const char* path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 3) {
        fprintf(stderr, "usage: stomp_plan.exe <victim_short> <payload.sys>\n");
        return 64;
    }
    wchar_t victimW[64]{};
    MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, victimW, 64);
    stomp::VictimImage vic{};
    if (!stomp::EnumerateVictimFromDisk(victimW, vic)) {
        fprintf(stderr, "[!] EnumerateVictimFromDisk failed victim=%s\n", argv[1]);
        return 2;
    }
    fprintf(stdout, "stage=ENUMERATE victim=%ls text_rva=0x%x text_vsize=0x%x\n",
        vic.name.c_str(), vic.text_rva, vic.text_vsize);
  if (!stomp::AnalyzeDeadCode(vic)) {
        fprintf(stderr, "[!] AnalyzeDeadCode failed dead_funcs=%zu\n", vic.dead_funcs.size());
        return 3;
    }
    fprintf(stdout, "stage=DEAD_CODE_SCAN dead_funcs=%zu\n", vic.dead_funcs.size());
    std::vector<uint8_t> payload = ReadBin(argv[2]);
    if (payload.empty()) {
        fprintf(stderr, "[!] payload read failed %s\n", argv[2]);
        return 4;
    }
    uint32_t sizeOfImage = stomp::PeSizeOfImage(payload);
    if (!sizeOfImage) {
        fprintf(stderr, "[!] invalid payload PE\n");
        return 4;
    }
    stomp::StompPlan plan{};
    plan.victim_name = vic.name;
    if (!stomp::PlanPlacement(vic, sizeOfImage, plan)) {
        uint32_t textEnd = vic.text_rva + vic.text_vsize;
        uint32_t tail = (textEnd > vic.text_used_high_rva)
            ? textEnd - vic.text_used_high_rva : 0;
        fprintf(stderr, "[!] PlanPlacement failed need=0x%x dead=%zu text_high=0x%x tail~0x%x max_cc=0x%x max_safe=0x%x\n",
            sizeOfImage, vic.dead_funcs.size(), vic.text_used_high_rva, tail,
            stomp::MaxTextCcRun(vic), stomp::MaxSafeTextSpan(vic, sizeOfImage));
        return 5;
    }
    fprintf(stdout,
        "stage=PLAN cluster_rva=0x%x need=0x%x thunk=0x%x payload_pe=0x%x preferred_base=0x%llx\n",
        plan.payload_rva, plan.payload_size, plan.thunk_rva, sizeOfImage,
        (unsigned long long)vic.preferred_base);
    return 0;
}