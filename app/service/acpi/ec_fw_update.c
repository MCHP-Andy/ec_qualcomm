
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "acpi_tbl.h"

LOG_MODULE_DECLARE(acpi);

// clang-format off
// ACPI_CMD_SUBSCRIBE(fw, EC_DEV_FW_CORRUPTION_STATUS, NULL, 1, 0, 2); // Page 39
// ACPI_CMD_SUBSCRIBE(fw, EC_FW_CODE_MIRROR, NULL, 3, 0, 0); // Page 40 (ByteCount + CodeMirrorCmd)
// ACPI_CMD_SUBSCRIBE(fw, EC_READ_CRC, NULL, 1, 0, 6); // Page 46
// ACPI_CMD_SUBSCRIBE(fw, EC_STATE_AND_WP_STATUS, NULL, 1, 0, 3); // Page 47
// ACPI_CMD_SUBSCRIBE(fw, EC_ERASE_MEM_REGION, NULL, 7, 0, 0); // Page 48 (ByteCount + ECControlPath + BlockCount + Address(3))
// ACPI_CMD_SUBSCRIBE(fw, EC_ERASE_MEM_PARTITION, NULL, 3, 0, 0); // Page 49 (ByteCount + QuickEraseCmd)
// ACPI_CMD_SUBSCRIBE(fw, EC_READ_MEM_REGION, NULL, 6, 0, ACPI_RESP_LEN); // Page 50 (ByteCount + ECControlPath + Address(3))
// EC_READ_MEM_REGION_BUF (0xA1) is a response command, not a top-level request command.
// ACPI_CMD_SUBSCRIBE(fw, EC_WRITE_MEM_REGION, NULL, 1, ACPI_RECE_LEN - 1, 0); // Page 51 (Optional Data(variable))
// ACPI_CMD_SUBSCRIBE(fw, EC_WRITE_MEM_REGION_BUF, NULL, 7, 0, 0); // Page 51 (ByteCount + ECControlPath + Size + Address(3))
// clang-format on
