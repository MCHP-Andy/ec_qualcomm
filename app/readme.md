# EC Firmware For Qualcomm


## 1. System Architecture Diagram
```mermaid
graph TD
    %% Top Level: Host SoC
    subgraph SOC_ZONE [SoC]
        direction TB
        SOC_ACPI[ACPI]
        SOC_SOCCP[SoCCP]
    end

    subgraph DEV_ZONE [Device]
        direction TB
        DEV_Fan[Fan]
        DEV_Therm[Thermistor]
    end

    %% Bottom Level: Embedded Controller
    subgraph EC_ZONE [EC]
        direction TB
        
        subgraph PHY_LAYER [Peripheral]
            direction TB
            EC_I2C[I2C]
            EC_PWM[PWM]
            EC_TACH[TACH]
            EC_ADC[ADC]
        end

        subgraph SVC_LAYER [Service]
            direction TB
            S_ACPI[ACPI]
            S_Thermal[Thermal]
            S_Fan[Fan]
            S_Power[Power]
        end

        PHY_LAYER <--Driver--> SVC_LAYER
    end

    %% Inter-chip & Internal communication
    SOC_ZONE <--> EC_ZONE
    DEV_ZONE <--> EC_ZONE

    %% Styling and Colors
    style SOC_ZONE fill:#f5f5f5,stroke:#333,stroke-width:2px
    style EC_ZONE fill:#fff,stroke:#333,stroke-width:2px
    style PHY_LAYER fill:#e8f5e9,stroke:#2e7d32
    style SVC_LAYER fill:#e3f2fd,stroke:#1565c0

    classDef socNode fill:#cfd8dc,stroke:#455a64,stroke-width:1px
    classDef ifNode fill:#c8e6c9,stroke:#388e3c,stroke-width:1px
    classDef svcNode fill:#bbdefb,stroke:#1976d2,stroke-width:1px

    class SOC_ACPI,SOC_SOCCP socNode
    class EC_I2C ifNode
    class S_ACPI,S_Thermal,S_Fan,S_Power svcNode
```

## 2. Directory Responsibilities

App 與 bootloader 共用 repository 根目錄下的 interface / driver / service，
`CONFIG_APP_BOOTLOADER` (於 `bootloader/prj.conf` 啟用) 用來選擇 bootloader 專屬元件。

### 📂 `interface/` (Interface Layer)
Driver 與 service 共用的 API 定義。`acpi.h`、`storage.h` 中 bootloader 專屬的
部分 (buffer 大小、image header) 以 `CONFIG_APP_BOOTLOADER` 區分。

### 📂 `service/` (Core Logic Layer)
ACPI service 由兩者共用；`ec_fw_update_bootloader.c` 與 `flash/` 僅 bootloader 使用，
SCI、Fan、Thermal、Power、SoCCP 等 service 僅 app 使用。

### 📂 `driver/` (Driver Layer / Infrastructure)
`acpi.c` (I2C target) 與 QEMU 用的 `simulator.c` 由兩者共用；`flash.c` 與
`storage_bootloader.c` 僅 bootloader 使用，其餘周邊 driver 僅 app 使用。
