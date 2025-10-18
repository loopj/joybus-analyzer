#pragma once

#include <LogicPublicTypes.h>

// Bus frequencies
#define JOYBUS_FREQ_CONSOLE  200000
#define JOYBUS_FREQ_GCC      250000
#define JOYBUS_FREQ_WAVEBIRD 225000

// Bus timing constants
#define JOYBUS_BUS_IDLE_NS      100000
#define JOYBUS_REPLY_TIMEOUT_NS 100000

// Maximum size of a Joybus transfer, in bytes
#define JOYBUS_BLOCK_SIZE 64

// Joybus command codes
#define JOYBUS_CMD_RESET               0xFF
#define JOYBUS_CMD_IDENTIFY            0x00
#define JOYBUS_CMD_N64_READ            0x01
#define JOYBUS_CMD_N64_ACCESSORY_READ  0x02
#define JOYBUS_CMD_N64_ACCESSORY_WRITE 0x03
#define JOYBUS_CMD_N64_KEYBOARD_READ   0x13
#define JOYBUS_CMD_GBA_READ            0x14
#define JOYBUS_CMD_GBA_WRITE           0x15
#define JOYBUS_CMD_PIXELFX_GAMEID      0x1D
#define JOYBUS_CMD_GCN_READ            0x40
#define JOYBUS_CMD_GCN_READ_ORIGIN     0x41
#define JOYBUS_CMD_GCN_CALIBRATE       0x42
#define JOYBUS_CMD_GCN_READ_LONG       0x43
#define JOYBUS_CMD_GCN_PROBE_DEVICE    0x4D
#define JOYBUS_CMD_GCN_FIX_DEVICE      0x4E
#define JOYBUS_CMD_GCN_KEYBOARD_READ   0x54

// Joybus command transfer lengths
#define JOYBUS_CMD_RESET_TX               1
#define JOYBUS_CMD_RESET_RX               3
#define JOYBUS_CMD_IDENTIFY_TX            1
#define JOYBUS_CMD_IDENTIFY_RX            3
#define JOYBUS_CMD_N64_READ_TX            1
#define JOYBUS_CMD_N64_READ_RX            4
#define JOYBUS_CMD_N64_ACCESSORY_READ_TX  3
#define JOYBUS_CMD_N64_ACCESSORY_READ_RX  33
#define JOYBUS_CMD_N64_ACCESSORY_WRITE_TX 35
#define JOYBUS_CMD_N64_ACCESSORY_WRITE_RX 1
#define JOYBUS_CMD_N64_KEYBOARD_READ_TX   2
#define JOYBUS_CMD_N64_KEYBOARD_READ_RX   7
#define JOYBUS_CMD_GBA_READ_TX            3
#define JOYBUS_CMD_GBA_READ_RX            33
#define JOYBUS_CMD_GBA_WRITE_TX           35
#define JOYBUS_CMD_GBA_WRITE_RX           1
#define JOYBUS_CMD_PIXELFX_GAMEID_TX      11
#define JOYBUS_CMD_PIXELFX_GAMEID_RX      0
#define JOYBUS_CMD_GCN_READ_TX            3
#define JOYBUS_CMD_GCN_READ_RX            8
#define JOYBUS_CMD_GCN_READ_ORIGIN_TX     1
#define JOYBUS_CMD_GCN_READ_ORIGIN_RX     10
#define JOYBUS_CMD_GCN_CALIBRATE_TX       3
#define JOYBUS_CMD_GCN_CALIBRATE_RX       10
#define JOYBUS_CMD_GCN_READ_LONG_TX       3
#define JOYBUS_CMD_GCN_READ_LONG_RX       10
#define JOYBUS_CMD_GCN_PROBE_DEVICE_TX    3
#define JOYBUS_CMD_GCN_PROBE_DEVICE_RX    8
#define JOYBUS_CMD_GCN_FIX_DEVICE_TX      3
#define JOYBUS_CMD_GCN_FIX_DEVICE_RX      3
#define JOYBUS_CMD_GCN_KEYBOARD_READ_TX   3
#define JOYBUS_CMD_GCN_KEYBOARD_READ_RX   8

namespace JoybusProtocol
{
    static inline U8 GetCommandTxLength( U8 command )
    {
        switch( command )
        {
        case JOYBUS_CMD_RESET:
            return JOYBUS_CMD_RESET_TX;
        case JOYBUS_CMD_IDENTIFY:
            return JOYBUS_CMD_IDENTIFY_TX;
        case JOYBUS_CMD_N64_READ:
            return JOYBUS_CMD_N64_READ_TX;
        case JOYBUS_CMD_N64_ACCESSORY_READ:
            return JOYBUS_CMD_N64_ACCESSORY_READ_TX;
        case JOYBUS_CMD_N64_ACCESSORY_WRITE:
            return JOYBUS_CMD_N64_ACCESSORY_WRITE_TX;
        case JOYBUS_CMD_N64_KEYBOARD_READ:
            return JOYBUS_CMD_N64_KEYBOARD_READ_TX;
        case JOYBUS_CMD_GBA_READ:
            return JOYBUS_CMD_GBA_READ_TX;
        case JOYBUS_CMD_GBA_WRITE:
            return JOYBUS_CMD_GBA_WRITE_TX;
        case JOYBUS_CMD_PIXELFX_GAMEID:
            return JOYBUS_CMD_PIXELFX_GAMEID_TX;
        case JOYBUS_CMD_GCN_READ:
            return JOYBUS_CMD_GCN_READ_TX;
        case JOYBUS_CMD_GCN_READ_ORIGIN:
            return JOYBUS_CMD_GCN_READ_ORIGIN_TX;
        case JOYBUS_CMD_GCN_CALIBRATE:
            return JOYBUS_CMD_GCN_CALIBRATE_TX;
        case JOYBUS_CMD_GCN_READ_LONG:
            return JOYBUS_CMD_GCN_READ_LONG_TX;
        case JOYBUS_CMD_GCN_PROBE_DEVICE:
            return JOYBUS_CMD_GCN_PROBE_DEVICE_TX;
        case JOYBUS_CMD_GCN_FIX_DEVICE:
            return JOYBUS_CMD_GCN_FIX_DEVICE_TX;
        case JOYBUS_CMD_GCN_KEYBOARD_READ:
            return JOYBUS_CMD_GCN_KEYBOARD_READ_TX;
        default:
            return 0;
        }
    }

    static inline U8 GetCommandRxLength( U8 command )
    {
        switch( command )
        {
        case JOYBUS_CMD_RESET:
            return JOYBUS_CMD_RESET_RX;
        case JOYBUS_CMD_IDENTIFY:
            return JOYBUS_CMD_IDENTIFY_RX;
        case JOYBUS_CMD_N64_READ:
            return JOYBUS_CMD_N64_READ_RX;
        case JOYBUS_CMD_N64_ACCESSORY_READ:
            return JOYBUS_CMD_N64_ACCESSORY_READ_RX;
        case JOYBUS_CMD_N64_ACCESSORY_WRITE:
            return JOYBUS_CMD_N64_ACCESSORY_WRITE_RX;
        case JOYBUS_CMD_N64_KEYBOARD_READ:
            return JOYBUS_CMD_N64_KEYBOARD_READ_RX;
        case JOYBUS_CMD_GBA_READ:
            return JOYBUS_CMD_GBA_READ_RX;
        case JOYBUS_CMD_GBA_WRITE:
            return JOYBUS_CMD_GBA_WRITE_RX;
        case JOYBUS_CMD_PIXELFX_GAMEID:
            return JOYBUS_CMD_PIXELFX_GAMEID_RX;
        case JOYBUS_CMD_GCN_READ:
            return JOYBUS_CMD_GCN_READ_RX;
        case JOYBUS_CMD_GCN_READ_ORIGIN:
            return JOYBUS_CMD_GCN_READ_ORIGIN_RX;
        case JOYBUS_CMD_GCN_CALIBRATE:
            return JOYBUS_CMD_GCN_CALIBRATE_RX;
        case JOYBUS_CMD_GCN_READ_LONG:
            return JOYBUS_CMD_GCN_READ_LONG_RX;
        case JOYBUS_CMD_GCN_PROBE_DEVICE:
            return JOYBUS_CMD_GCN_PROBE_DEVICE_RX;
        case JOYBUS_CMD_GCN_FIX_DEVICE:
            return JOYBUS_CMD_GCN_FIX_DEVICE_RX;
        case JOYBUS_CMD_GCN_KEYBOARD_READ:
            return JOYBUS_CMD_GCN_KEYBOARD_READ_RX;
        default:
            return 0;
        }
    }

    static inline bool CommandExists( U8 command )
    {
        return GetCommandTxLength( command ) != 0;
    }
}
