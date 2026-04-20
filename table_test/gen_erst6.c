// gen_erst6.c - Full ERST table generator with read/write support
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#pragma pack(push, 1)
struct acpi_generic_address {
    u8 space_id;
    u8 bit_width;
    u8 bit_offset;
    u8 access_width;
    u64 address;
};

struct acpi_whea_header {
    u8 action;
    u8 instruction;
    u8 flags;
    u8 reserved;
    struct acpi_generic_address register_region;
    u64 value;
    u64 mask;
};

struct acpi_table_header {
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char oem_id[6];
    char oem_table_id[8];
    u32 oem_revision;
    char asl_compiler_id[4];
    u32 asl_compiler_revision;
};

struct acpi_table_erst {
    struct acpi_table_header header;
    u32 header_length;
    u32 reserved;
    u32 entries;
};
#pragma pack(pop)

// === Actions (from actbl1.h) ===
#define ACPI_ERST_BEGIN_WRITE           0
#define ACPI_ERST_BEGIN_READ            1
#define ACPI_ERST_BEGIN_CLEAR           2
#define ACPI_ERST_END                   3
#define ACPI_ERST_SET_RECORD_OFFSET     4
#define ACPI_ERST_EXECUTE_OPERATION     5
#define ACPI_ERST_CHECK_BUSY_STATUS     6
#define ACPI_ERST_GET_COMMAND_STATUS    7
#define ACPI_ERST_GET_RECORD_ID         8
#define ACPI_ERST_SET_RECORD_ID         9
#define ACPI_ERST_GET_RECORD_COUNT      10
#define ACPI_ERST_BEGIN_DUMMY_WRITE     11
#define ACPI_ERST_NOT_USED              12
#define ACPI_ERST_GET_ERROR_RANGE       13
#define ACPI_ERST_GET_ERROR_LENGTH      14
#define ACPI_ERST_GET_ERROR_ATTRIBUTES  15

// Instructions
#define ACPI_ERST_READ_REGISTER         0
#define ACPI_ERST_READ_REGISTER_VALUE   1
#define ACPI_ERST_WRITE_REGISTER        2
#define ACPI_ERST_WRITE_REGISTER_VALUE  3

// Hardware layout
#define ERST_WORKSPACE_BASE     0x87003000ULL
#define ERROR_LOG_BUFFER_BASE   0x87004000ULL
#define ERROR_LOG_SIZE          0x2000ULL // 8KB

// Register offsets within workspace
#define CMD_REG_OFFSET          0x04    // Command/status (32-bit)
#define OFFSET_REG_OFFSET       0x08    // Record offset (32-bit)
#define DATA_REG_OFFSET         0x10    // Data (64-bit, optional)
#define ATTR_REG_OFFSET         0x20    // Attributes (32-bit)
#define RECORD_ID_REG_OFFSET    0x28    // Record ID (64-bit)

// GAS helper
#define GAS_SYSMEM(addr, width) \
    (struct acpi_generic_address) { \
        .space_id = 0, \
        .bit_width = width, \
        .bit_offset = 0, \
        .access_width = (width == 64) ? 4 : 3, \
        .address = addr \
    }

int main(void) {
    const char *output_file = "erst.aml";
    const int num_entries = 12; // Full set for write + read
    size_t entry_size = sizeof(struct acpi_whea_header);
    size_t table_size = sizeof(struct acpi_table_erst) + num_entries * entry_size;

    unsigned char *buf = calloc(1, table_size);
    if (!buf) {
        perror("calloc");
        return 1;
    }

    struct acpi_table_erst *erst = (struct acpi_table_erst *)buf;
    memcpy(erst->header.signature, "ERST", 4);
    erst->header.length = table_size;
    erst->header.revision = 1;
    memcpy(erst->header.oem_id, "INTEL ", 6);
    memcpy(erst->header.oem_table_id, "ERSTDBG", 8);
    erst->header.oem_revision = 1;
    memcpy(erst->header.asl_compiler_id, "INTL", 4);
    erst->header.asl_compiler_revision = 0x20251212;
    erst->header_length = sizeof(struct acpi_table_erst) - sizeof(struct acpi_table_header);
    erst->reserved = 0;
    erst->entries = num_entries;

    struct acpi_whea_header *entry = (struct acpi_whea_header *)(buf + sizeof(struct acpi_table_erst));

    // --- 1. Get log buffer info (MUST be actions 13, 14, 15) ---
    entry[0] = (struct acpi_whea_header){
        .action = ACPI_ERST_GET_ERROR_RANGE,        // 13
        .instruction = ACPI_ERST_READ_REGISTER,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + 0x00, 64),
        .value = ERROR_LOG_BUFFER_BASE,
        .mask = ~0ULL
    };

    entry[1] = (struct acpi_whea_header){
        .action = ACPI_ERST_GET_ERROR_LENGTH,       // 14
        .instruction = ACPI_ERST_READ_REGISTER,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + 0x08, 32),
        .value = ERROR_LOG_SIZE,
        .mask = ~0ULL
    };

    entry[2] = (struct acpi_whea_header){
        .action = ACPI_ERST_GET_ERROR_ATTRIBUTES,   // 15
        .instruction = ACPI_ERST_READ_REGISTER,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + ATTR_REG_OFFSET, 32),
        .value = 0, // Not NVRAM
        .mask = ~0ULL
    };

    // --- 2. Write operations ---
    entry[3] = (struct acpi_whea_header){
        .action = ACPI_ERST_BEGIN_WRITE,
        .instruction = ACPI_ERST_WRITE_REGISTER_VALUE,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + CMD_REG_OFFSET, 32),
        .value = 1,
        .mask = ~0ULL
    };

    entry[4] = (struct acpi_whea_header){
        .action = ACPI_ERST_SET_RECORD_OFFSET,
        .instruction = ACPI_ERST_WRITE_REGISTER_VALUE,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + OFFSET_REG_OFFSET, 32),
        .value = 0, // Input from ctx
        .mask = ~0ULL
    };

    entry[5] = (struct acpi_whea_header){
        .action = ACPI_ERST_EXECUTE_OPERATION,
        .instruction = ACPI_ERST_WRITE_REGISTER_VALUE,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + CMD_REG_OFFSET, 32),
        .value = 2, // Execute
        .mask = ~0ULL
    };

    entry[6] = (struct acpi_whea_header){
        .action = ACPI_ERST_CHECK_BUSY_STATUS,
        .instruction = ACPI_ERST_READ_REGISTER_VALUE,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + CMD_REG_OFFSET, 32),
        .value = 0, // Always return not busy
        .mask = 1ULL
    };

    entry[7] = (struct acpi_whea_header){
        .action = ACPI_ERST_GET_COMMAND_STATUS,
        .instruction = ACPI_ERST_READ_REGISTER_VALUE,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + CMD_REG_OFFSET, 32),
        .value = 0, // Success
        .mask = ~0ULL
    };

    entry[8] = (struct acpi_whea_header){
        .action = ACPI_ERST_END,
        .instruction = ACPI_ERST_WRITE_REGISTER_VALUE,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + CMD_REG_OFFSET, 32),
        .value = 3, // End
        .mask = ~0ULL
    };

    // --- 3. Read operations ---
    entry[9] = (struct acpi_whea_header){
        .action = ACPI_ERST_BEGIN_READ,
        .instruction = ACPI_ERST_WRITE_REGISTER_VALUE,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + CMD_REG_OFFSET, 32),
        .value = 4, // Begin Read
        .mask = ~0ULL
    };

    entry[10] = (struct acpi_whea_header){
        .action = ACPI_ERST_SET_RECORD_ID,
        .instruction = ACPI_ERST_WRITE_REGISTER_VALUE,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + RECORD_ID_REG_OFFSET, 64),
        .value = 0, // Input from ctx
        .mask = ~0ULL
    };

    entry[11] = (struct acpi_whea_header){
        .action = ACPI_ERST_GET_RECORD_ID,
        .instruction = ACPI_ERST_READ_REGISTER_VALUE,
        .flags = 0,
        .register_region = GAS_SYSMEM(ERST_WORKSPACE_BASE + RECORD_ID_REG_OFFSET, 64),
        .value = 0, // Default: no record (set to non-zero after write to enable read)
        .mask = ~0ULL
    };

    // Compute checksum
    u8 sum = 0;
    for (size_t i = 0; i < table_size; i++) {
        sum += buf[i];
    }
    erst->header.checksum = (u8)(0 - sum);

    FILE *f = fopen(output_file, "wb");
    if (!f) {
        perror("fopen");
        free(buf);
        return 1;
    }
    fwrite(buf, 1, table_size, f);
    fclose(f);
    free(buf);

    printf("Generated %s (%zu bytes)\n", output_file, table_size);
    printf("Use with QEMU: -acpitable file=%s\n", output_file);
    printf("\nInitialize these physical addresses:\n");
    printf(" 0x%llx = 0x%llx # Error log base\n", ERST_WORKSPACE_BASE + 0x00, ERROR_LOG_BUFFER_BASE);
    printf(" 0x%llx = 0x%llx # Error log size\n", ERST_WORKSPACE_BASE + 0x08, ERROR_LOG_SIZE);
    printf(" 0x%llx = 0 # Command/status register\n", ERST_WORKSPACE_BASE + CMD_REG_OFFSET);
    printf(" 0x%llx = 0 # Record ID register (set to non-zero after write to enable read)\n",
           ERST_WORKSPACE_BASE + RECORD_ID_REG_OFFSET);

    return 0;
}
