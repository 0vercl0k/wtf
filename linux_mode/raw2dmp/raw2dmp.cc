#include "../../src/libs/kdmp-parser/src/lib/kdmp-parser-structs.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstring>
#include <stdio.h>

namespace fs = std::filesystem;
using namespace kdmpparser;

#define PAGE_4K 4096

int main(int argc, char** argv)
{
    if(argc == 1){
        std::cout << "converter name" << std::endl;
        return 0;
    }

    fs::path raw_dump_file = argv[1];

    if(!fs::exists(raw_dump_file) || !fs::is_regular_file(raw_dump_file)){
        std::cout << raw_dump_file << " does not exists" << std::endl;
        return 0;
    }

    fs::path dump_dir = fs::path(raw_dump_file).parent_path();
    fs::path memdmp = dump_dir / "mem.dmp";

    if(raw_dump_file == memdmp){
        std::cout << "dump file cannot be 'mem.dmp', rename it" << std::endl;
        return 0;
    }

    uint64_t dump_size   = fs::file_size(raw_dump_file);
    uint64_t pages_count = dump_size / PAGE_4K;
    uint64_t bitmap_size = pages_count / 8;

    uint8_t* bitmap = (uint8_t*)malloc(bitmap_size);
    std::memset(bitmap, 0xff, bitmap_size);

    uint64_t header_size = sizeof(HEADER64) - 1; // uint8_t Bitmap[1] not used

    HEADER64* header = (HEADER64*)malloc(header_size);

    header->Signature = header->ExpectedSignature;
    header->ValidDump = header->ExpectedValidDump;
    header->DumpType  = DumpType_t::BMPDump;

    header->BmpHeader.Signature = header->BmpHeader.ExpectedSignature;
    header->BmpHeader.ValidDump = header->BmpHeader.ExpectedValidDump;
    header->BmpHeader.FirstPage = header_size + bitmap_size;
    header->BmpHeader.Pages = pages_count;

    header->ContextRecord.MxCsr = header->ContextRecord.MxCsr2;

    std::ofstream memdmp_stream(memdmp.c_str(), std::ios_base::binary);

    memdmp_stream.write((char*)header, header_size);
    memdmp_stream.write((char*)bitmap, bitmap_size);

    std::ifstream raw_stream(raw_dump_file, std::ios_base::binary);

    memdmp_stream << raw_stream.rdbuf();

    free(bitmap);
    free(header);

    fs::remove(raw_dump_file);

    return 0;
}