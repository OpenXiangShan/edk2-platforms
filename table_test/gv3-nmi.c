// gen_einj_full.c
// Final version: based on gv3.c + XiangShan NMI injection support
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#pragma pack(push, 1)

// --- ACPI TABLE HEADER ---
struct acpi_table_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    char asl_compiler_id[4];
    uint32_t asl_compiler_revision;
};

// --- GENERIC ADDRESS ---
struct acpi_generic_address {
    uint8_t space_id;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_width;
    uint64_t address;
};

// --- Error Record Structure (from actbl1.h) ---
struct acpi_hest_generic_status {
    uint32_t block_status;
    uint32_t raw_data_offset;
    uint32_t raw_data_length;
    uint32_t data_length;
    uint32_t error_severity;
};

// --- WHEA Header (used in EINJ table) ---
struct acpi_whea_header {
    uint8_t action;
    uint8_t instruction;
    uint8_t flags;
    uint8_t reserved;
    struct acpi_generic_address register_region;
    uint64_t value;
    uint64_t mask;
};

struct acpi_table_einj {
    struct acpi_table_header header;
    uint32_t header_length;
    uint8_t flags;
    uint8_t reserved[3];
    uint32_t entries;
};

struct acpi_einj_trigger {
    uint32_t header_size;
    uint32_t revision;
    uint32_t table_size;
    uint32_t entry_count;
};

struct acpi_einj_trigger_entry {
    uint8_t action;
    uint8_t instruction;
    uint8_t flags;
    uint8_t reserved;
    struct acpi_generic_address register_region;
    uint64_t value;
    uint64_t mask;
};

#pragma pack(pop)

// === MEMORY LAYOUT (from gv3.c) ===
#define EINJ_REGISTERS_BASE     0x87000000ULL
#define TRIGGER_TABLE_OFFSET    0x1000
#define ERROR_STATUS_OFFSET     0x1200
#define ERROR_RECORD_OFFSET     0x1300
#define TRIGGER_TABLE_ADDR      (EINJ_REGISTERS_BASE + TRIGGER_TABLE_OFFSET)
#define ERROR_STATUS_ADDR       (EINJ_REGISTERS_BASE + ERROR_STATUS_OFFSET)
#define ERROR_RECORD_ADDR       (EINJ_REGISTERS_BASE + ERROR_RECORD_OFFSET)
#define WORKSPACE_SIZE          0x2000

// Register offsets
#define EXEC_OFFSET             0x00
#define ERROR_TYPE_OFFSET       0x08
#define BUSY_OFFSET             0x10
#define TRIGGER_ADDR_REG_OFFSET 0x18

// XiangShan ECC Controller Registers (from your test code)
#define ECCCTL_ADDR             0x38022000ULL
#define ECCEID_ADDR             0x38022008ULL
#define ECCMASK_ADDR            0x38022010ULL
#define ECCMASK2_ADDR           (ECCMASK_ADDR + 2*8) // ECCMASK[2]

// Error types
#define ERR_MEM_CORRECTABLE       (1ULL << 3)
#define ERR_MEM_UNCORR_NONFATAL   (1ULL << 4)
#define ERR_MEM_UNCORR_FATAL      (1ULL << 5)
#define ERR_PCIE_CORRECTABLE      (1ULL << 6)
#define SUPPORTED_ERROR_TYPES     (ERR_MEM_CORRECTABLE | ERR_MEM_UNCORR_NONFATAL | \
                                   ERR_MEM_UNCORR_FATAL | ERR_PCIE_CORRECTABLE)

int main(void) {
    // === Step 1: Build ACPI EINJ Table (same as gv3.c) ===
    struct acpi_whea_header einj_entries[] = {
        // BEGIN_OPERATION
        { .action = 0, .instruction = 2, .flags = 0,
          .register_region = {0, 0x40, 0, 4, EINJ_REGISTERS_BASE + EXEC_OFFSET},
          .value = 1, .mask = ~0ULL },
        // GET_TRIGGER_TABLE
        { .action = 1, .instruction = 0, .flags = 0,
          .register_region = {0, 0x40, 0, 4, EINJ_REGISTERS_BASE + TRIGGER_ADDR_REG_OFFSET},
          .value = TRIGGER_TABLE_ADDR, .mask = ~0ULL },
        // SET_ERROR_TYPE — ONLY ONE ENTRY!
        { .action = 2, .instruction = 2, .flags = 0,
          .register_region = {0, 0x20, 0, 3, EINJ_REGISTERS_BASE + ERROR_TYPE_OFFSET},
          .value = 0, .mask = ~0ULL },
        // GET_ERROR_TYPE
        { .action = 3, .instruction = 0, .flags = 0,
          .register_region = {0, 0x20, 0, 3, EINJ_REGISTERS_BASE + ERROR_TYPE_OFFSET},
          .value = 0, .mask = ~0ULL },
        // END_OPERATION
        { .action = 4, .instruction = 4, .flags = 0,
          .register_region = {0, 0x40, 0, 4, EINJ_REGISTERS_BASE + EXEC_OFFSET},
          .value = 0, .mask = 0ULL },
        // EXECUTE_OPERATION
        { .action = 5, .instruction = 2, .flags = 0,
          .register_region = {0, 8, 0, 1, EINJ_REGISTERS_BASE + BUSY_OFFSET},
          .value = 2, .mask = ~0ULL },
        // CHECK_BUSY_STATUS
        { .action = 6, .instruction = 0, .flags = 0,
          .register_region = {0, 8, 0, 1, EINJ_REGISTERS_BASE + BUSY_OFFSET},
          .value = 0, .mask = 1ULL },
        // GET_COMMAND_STATUS
        { .action = 7, .instruction = 0, .flags = 0,
          .register_region = {0, 8, 0, 1, ERROR_STATUS_ADDR},
          .value = 0, .mask = 0ULL },
        // SET_ERROR_TYPE_WITH_ADDRESS
        { .action = 8, .instruction = 2, .flags = 0,
          .register_region = {0, 0x40, 0, 4, EINJ_REGISTERS_BASE + 0x200},
          .value = ERR_MEM_CORRECTABLE, .mask = ~0ULL }
    };

    struct acpi_table_einj einj_tbl = {
        .header = {
            .signature = "EINJ", .revision = 1,
            .oem_id = "OEMID ", .oem_table_id = "EINJ_XS",
            .oem_revision = 1, .asl_compiler_id = "INTL",
            .asl_compiler_revision = 0x20251212
        },
        .header_length = sizeof(struct acpi_table_einj) - sizeof(struct acpi_table_header),
        .flags = 0,
        .entries = sizeof(einj_entries) / sizeof(einj_entries[0])
    };

    size_t total_size = sizeof(struct acpi_table_header) +
                        (sizeof(einj_tbl) - sizeof(struct acpi_table_header)) +
                        sizeof(einj_entries);
    einj_tbl.header.length = total_size;

    uint8_t *buf = malloc(total_size);
    memcpy(buf, &einj_tbl.header, sizeof(struct acpi_table_header));
    memcpy(buf + sizeof(struct acpi_table_header), &einj_tbl.header_length,
           sizeof(einj_tbl) - sizeof(struct acpi_table_header));
    memcpy(buf + sizeof(struct acpi_table_header) + sizeof(einj_tbl) - sizeof(struct acpi_table_header),
           einj_entries, sizeof(einj_entries));

    uint8_t sum = 0;
    for (size_t i = 0; i < total_size; i++) sum += buf[i];
    buf[9] = (uint8_t)(0x100 - sum);

    FILE *f = fopen("EINJ.aml", "wb");
    fwrite(buf, 1, total_size, f);
    fclose(f);
    free(buf);
    printf("[+] EINJ.aml generated (%zu bytes)\n", total_size);

    // === Step 2: Build einj_workspace.bin with NMI trigger ===
    uint8_t workspace[WORKSPACE_SIZE] = {0};

    // --- Link ERROR_STATUS to ERROR_RECORD (critical for GHES) ---
    *(uint64_t*)(workspace + ERROR_STATUS_OFFSET) = ERROR_RECORD_ADDR;

    // --- Enhanced Trigger Table: Inject ECC error + set error record ---
    struct acpi_einj_trigger_entry trigger_entries[] = {
        // 1. ECCMASK[2] = 0xff
        { .action = 0xFF, .instruction = 0x03, .flags = 0,
          .register_region = {0, 0x40, 0, 4, ECCMASK2_ADDR},
          .value = 0xff, .mask = ~0ULL },
        // 2. ECCEID = 0x6
        { .action = 0xFF, .instruction = 0x03, .flags = 0,
          .register_region = {0, 0x40, 0, 4, ECCEID_ADDR},
          .value = 0x6, .mask = ~0ULL },
        // 3. ECCCTL = 0x4d (trigger)
        { .action = 0xFF, .instruction = 0x03, .flags = 0,
          .register_region = {0, 0x40, 0, 4, ECCCTL_ADDR},
          .value = 0x4d, .mask = ~0ULL },
        // 4. Set error record valid (for GHES polling)
        { .action = 0xFF, .instruction = 0x03, .flags = 0,
          .register_region = {0, 0x40, 0, 4, ERROR_RECORD_ADDR},
          .value = 0x1, .mask = ~0ULL },
        // 5. Write ERROR_STATUS_ADDR = ERROR_RECORD_ADDR (redundant but safe)
        { .action = 0xFF, .instruction = 0x03, .flags = 0,
          .register_region = {0, 0x40, 0, 4, ERROR_STATUS_ADDR},
          .value = ERROR_RECORD_ADDR, .mask = ~0ULL }
    };

    struct acpi_einj_trigger trigger_hdr = {
        .header_size = sizeof(struct acpi_einj_trigger),
        .revision = 1,
        .entry_count = sizeof(trigger_entries) / sizeof(trigger_entries[0])
    };
    trigger_hdr.table_size = sizeof(trigger_hdr) + sizeof(trigger_entries);

    memcpy(workspace + TRIGGER_TABLE_OFFSET, &trigger_hdr, sizeof(trigger_hdr));
    memcpy(workspace + TRIGGER_TABLE_OFFSET + sizeof(trigger_hdr),
           trigger_entries, sizeof(trigger_entries));

    // --- Main Registers (same as gv3.c) ---
    uint64_t exec_val = 1;
    memcpy(workspace + EXEC_OFFSET, &exec_val, 8);

    uint64_t error_type_val = SUPPORTED_ERROR_TYPES;
    memcpy(workspace + ERROR_TYPE_OFFSET, &error_type_val, 8);

    uint64_t trigger_table_addr = TRIGGER_TABLE_ADDR;
    memcpy(workspace + TRIGGER_ADDR_REG_OFFSET, &trigger_table_addr, 8);

    workspace[BUSY_OFFSET] = 0;

    // --- Pre-fill Error Record (optional, but safe) ---
    struct acpi_hest_generic_status *error_record =
        (struct acpi_hest_generic_status *)(workspace + ERROR_RECORD_OFFSET);
    error_record->block_status = 0x1;        // Will be overwritten by trigger, but OK
    error_record->raw_data_offset = 0;
    error_record->raw_data_length = 0;
    error_record->error_severity = 1;        // non-fatal (recoverable but serious)
    error_record->data_length = 0;

    // Write workspace
    f = fopen("einj_workspace.bin", "wb");
    fwrite(workspace, 1, sizeof(workspace), f);
    fclose(f);
    printf("[+] einj_workspace.bin generated\n");
    printf(" - Trigger table at: 0x%llx\n", TRIGGER_TABLE_ADDR);
    printf(" - Error record at:  0x%llx\n", ERROR_RECORD_ADDR);
    printf(" - NMI will be triggered via ECC controller\n");

    return 0;
}