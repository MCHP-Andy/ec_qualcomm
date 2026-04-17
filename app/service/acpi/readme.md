# ACPI service

## Architecture
```mermaid
graph LR
    subgraph "Service (Core Logic)"
        Wait[Wait for Command]
        Search[Table Lookup]
        Handle[Execute Handler]
    end

    subgraph "Data Transport & Storage"
        MQ[(acpi_evt_queue)]
        BUF[resp_buf]
    end

    subgraph "Interface (Input/Output)"
        IN[acpi_write] 
        OUT[acpi_read]
    end

    %% Input Flow
    IN -->|Pack Event| MQ
    MQ -->|k_msgq_get| Wait
    
    %% Handling Flow
    Wait --> Search
    Search -->|Match Found| Handle
    
    %% Output Flow
    Handle -->|Write Results| BUF
    BUF -.->|memcpy| OUT
```

## Test
```shell
uart:~$
uart:~$ acpi read
ACPI Read Response:
00000000: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$
uart:~$
uart:~$ acpi write 0x0E
ACPI CMD written successfully
[00:05:27.850,738] <inf> acpi: Handled ACPI cmd 0x0e successfully
uart:~$
uart:~$
uart:~$ acpi read
ACPI Read Response:
00000000: 03 01 01 01 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........|
uart:~$
```
