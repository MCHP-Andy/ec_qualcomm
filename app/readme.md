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

The app and bootloader share their interfaces, drivers, and services from the
repository root. `CONFIG_EC_BOOTLOADER` selects bootloader-only components.

### 📂 `interface/` (Shared Interface Layer)
Public APIs used by the shared drivers and services.

### 📂 `service/` (Shared and App Services)
ACPI command handling is shared; SCI event queueing is provided by the separate
`service/sci` service. Fan, thermal, power, and SoCCP services are app-only.

### 📂 `driver/` (Shared and App Drivers)
Shared ACPI and storage drivers plus app-only peripheral drivers. Bootloader
flash and image-validation implementations are selected by
`CONFIG_EC_BOOTLOADER`.
