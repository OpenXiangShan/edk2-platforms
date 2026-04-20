// gen_hest.c - CORRECTED VERSION
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

void put_le16(uint8_t *buf, size_t off, uint16_t val) {
    buf[off] = val & 0xff;
    buf[off+1] = (val >> 8) & 0xff;
}

void put_le32(uint8_t *buf, size_t off, uint32_t val) {
    for (int i = 0; i < 4; i++)
        buf[off+i] = (val >> (i*8)) & 0xff;
}

void put_le64(uint8_t *buf, size_t off, uint64_t val) {
    for (int i = 0; i < 8; i++)
        buf[off+i] = (val >> (i*8)) & 0xff;
}

int main(void) {
    // === CRITICAL: MUST MATCH EINJ's ERROR_STATUS_ADDR ===
    const uint64_t error_addr = 0x87001200ULL; // ← ONLY CORRECT VALUE
    
    const size_t table_header_size = 36;
    const size_t ghes_v1_size = 64;
    size_t total_size = table_header_size + 4 + ghes_v1_size;

    uint8_t *buf = calloc(1, total_size);

    // --- ACPI Table Header ---
    memcpy(buf + 0, "HEST", 4);
    put_le32(buf, 4, total_size);
    buf[8] = 1;
    memcpy(buf + 10, "OEMID ", 6);
    memcpy(buf + 16, "HESTGEN", 8);
    put_le32(buf, 24, 1);
    memcpy(buf + 28, "INTL", 4);
    put_le32(buf, 32, 0x20251212);

    // --- error_source_count = 1 ---
    put_le32(buf, table_header_size, 1);

    // --- Generic Error Source v1 ---
    size_t ghes_off = table_header_size + 4;
    put_le16(buf, ghes_off + 0x00, 0x0009);
    put_le16(buf, ghes_off + 0x02, 0x0001);
    put_le16(buf, ghes_off + 0x04, 0xFFFF);
    buf[ghes_off + 0x06] = 0x00;
    buf[ghes_off + 0x07] = 0x01;
    put_le32(buf, ghes_off + 0x08, 1);
    put_le32(buf, ghes_off + 0x0C, 1);
    put_le32(buf, ghes_off + 0x10, 0x1000);

    // --- ERROR STATUS ADDRESS (MUST BE 0x87001200) ---
    buf[ghes_off + 0x14] = 0; // space_id = SystemMemory
    buf[ghes_off + 0x15] = 0x40; // bit_width = 64
    buf[ghes_off + 0x16] = 0;
    buf[ghes_off + 0x17] = 4; // access_width = QWORD
    put_le64(buf, ghes_off + 0x18, error_addr); // ← THIS IS THE FIX

    // notify structure
    buf[ghes_off + 0x20] = 0; // POLLED
    buf[ghes_off + 0x21] = 0x1C;
    buf[ghes_off + 0x24] = 0xA0; // poll_interval
    put_le32(buf, ghes_off + 0x3C, 0x1000);

    // --- Fix checksum ---
    uint8_t sum = 0;
    for (size_t i = 0; i < total_size; i++)
        sum += buf[i];
    buf[9] = (uint8_t)(0x100 - sum);

    // --- Write file ---
    FILE *f = fopen("HEST.aml", "wb");
    fwrite(buf, 1, total_size, f);
    fclose(f);
    free(buf);

    printf("[+] HEST.aml generated with Error Status Address: 0x%lx\n", error_addr);
    return 0;
}
