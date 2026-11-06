# Fan service

## Architecture
```mermaid
graph LR
    subgraph "Service (Core Logic)"
        Wait[Wait for Control Cmd]
        Calc[Duty Cycle Calculation]
        Apply[Apply PWM Settings]
    end

    subgraph "Data Storage"
        Status[(Fan Status Table)]
        Config[Fan Configuration]
    end

    subgraph "Interface (Input/Output)"
        IN[fan_set_speed]
        SHELL[fan shell cmd]
        DRV[PWM/Tacho Driver]
    end

    %% Control Flow
    IN --> Wait
    SHELL --> Wait
    Wait --> Calc
    
    %% Data Flow
    Calc --> Status
    Status --> Apply
    
    %% Hardware Interaction
    Apply -->|Set Duty| DRV
    DRV -.->|Get RPM| Status
```

## Test
```shell
uart:~$
uart:~$ fan info
Fan Index | Status  | Target % | Current RPM | Duty Cycle
-------------------------------------------------------
   0      | Running |   30%    |    1850     |   76/255
   1      | Idle    |    0%    |       0     |    0/255

uart:~$
uart:~$ fan set 0 80
Fan 0 speed set to 80% successfully
[00:12:45.102,345] <inf> fan: Adjusting Fan 0 PWM duty to 204
uart:~$
uart:~$ fan info
Fan Index | Status  | Target % | Current RPM | Duty Cycle
-------------------------------------------------------
   0      | Running |   80%    |    4520     |   204/255
   1      | Idle    |    0%    |       0     |    0/255

uart:~$
uart:~$ fan stop 0
Fan 0 stopped successfully
uart:~$
```