//***************************************************************************
// Si7021 module for ESP8266 with nodeMCU
// fetchbot @github
// MIT license, http://opensource.org/licenses/MIT
//***************************************************************************

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "osapi.h"
#include "c_stdlib.h"


//***************************************************************************
// CHIP
//***************************************************************************
#define ADS1115_ADS1015 ( 15)
#define ADS1115_ADS1115 (115)

//***************************************************************************
// I2C ADDRESS DEFINITON
//***************************************************************************

#define ADS1115_I2C_ADDR_GND        (0x48)
#define ADS1115_I2C_ADDR_VDD        (0x49)
#define ADS1115_I2C_ADDR_SDA        (0x4A)
#define ADS1115_I2C_ADDR_SCL        (0x4B)
#define IS_I2C_ADDR_VALID(addr) (((addr) & 0xFC) == 0x48)

//***************************************************************************
// POINTER REGISTER
//***************************************************************************

#define ADS1115_POINTER_MASK        (0x03)
#define ADS1115_POINTER_CONVERSION  (0x00)
#define ADS1115_POINTER_CONFIG      (0x01)
#define ADS1115_POINTER_THRESH_LOW  (0x02)
#define ADS1115_POINTER_THRESH_HI   (0x03)

//***************************************************************************
// CONFIG REGISTER
//***************************************************************************

#define ADS1115_OS_MASK             (0x8000)
#define ADS1115_OS_NON              (0x0000)
#define ADS1115_OS_SINGLE           (0x8000)    // Write: Set to start a single-conversion
#define ADS1115_OS_BUSY             (0x0000)    // Read: Bit = 0 when conversion is in progress
#define ADS1115_OS_NOTBUSY          (0x8000)    // Read: Bit = 1 when device is not performing a conversion

#define ADS1115_MUX_MASK            (0x7000)
#define ADS1115_MUX_DIFF_0_1        (0x0000)    // Differential P = AIN0, N = AIN1 (default)
#define ADS1115_MUX_DIFF_0_3        (0x1000)    // Differential P = AIN0, N = AIN3
#define ADS1115_MUX_DIFF_1_3        (0x2000)    // Differential P = AIN1, N = AIN3
#define ADS1115_MUX_DIFF_2_3        (0x3000)    // Differential P = AIN2, N = AIN3
#define ADS1115_MUX_SINGLE_0        (0x4000)    // Single-ended AIN0
#define ADS1115_MUX_SINGLE_1        (0x5000)    // Single-ended AIN1
#define ADS1115_MUX_SINGLE_2        (0x6000)    // Single-ended AIN2
#define ADS1115_MUX_SINGLE_3        (0x7000)    // Single-ended AIN3
#define IS_CHANNEL_VALID(channel) (((channel) & 0x8FFF) == 0)

#define ADS1115_PGA_MASK            (0x0E00)
#define ADS1115_PGA_6_144V          (0x0000)    // +/-6.144V range = Gain 2/3
#define ADS1115_PGA_4_096V          (0x0200)    // +/-4.096V range = Gain 1
#define ADS1115_PGA_2_048V          (0x0400)    // +/-2.048V range = Gain 2 (default)
#define ADS1115_PGA_1_024V          (0x0600)    // +/-1.024V range = Gain 4
#define ADS1115_PGA_0_512V          (0x0800)    // +/-0.512V range = Gain 8
#define ADS1115_PGA_0_256V          (0x0A00)    // +/-0.256V range = Gain 16

#define ADS1115_MODE_MASK           (0x0100)
#define ADS1115_MODE_CONTIN         (0x0000)    // Continuous conversion mode
#define ADS1115_MODE_SINGLE         (0x0100)    // Power-down single-shot mode (default)

#define ADS1115_DR_MASK             (0x00E0)
#define ADS1115_DR_8SPS             (   8)
#define ADS1115_DR_16SPS            (  16)
#define ADS1115_DR_32SPS            (  32)
#define ADS1115_DR_64SPS            (  64)
#define ADS1115_DR_128SPS           ( 128)
#define ADS1115_DR_250SPS           ( 250)
#define ADS1115_DR_475SPS           ( 475)
#define ADS1115_DR_490SPS           ( 490)
#define ADS1115_DR_860SPS           ( 860)
#define ADS1115_DR_920SPS           ( 920)
#define ADS1115_DR_1600SPS          (1600)
#define ADS1115_DR_2400SPS          (2400)
#define ADS1115_DR_3300SPS          (3300)

#define ADS1115_CMODE_MASK          (0x0010)
#define ADS1115_CMODE_TRAD          (0x0000)    // Traditional comparator with hysteresis (default)
#define ADS1115_CMODE_WINDOW        (0x0010)    // Window comparator

#define ADS1115_CPOL_MASK           (0x0008)
#define ADS1115_CPOL_ACTVLOW        (0x0000)    // ALERT/RDY pin is low when active (default)
#define ADS1115_CPOL_ACTVHI         (0x0008)    // ALERT/RDY pin is high when active

#define ADS1115_CLAT_MASK           (0x0004)    // Determines if ALERT/RDY pin latches once asserted
#define ADS1115_CLAT_NONLAT         (0x0000)    // Non-latching comparator (default)
#define ADS1115_CLAT_LATCH          (0x0004)    // Latching comparator

#define ADS1115_CQUE_MASK           (0x0003)
#define ADS1115_CQUE_1CONV          (0x0000)    // Assert ALERT/RDY after one conversions
#define ADS1115_CQUE_2CONV          (0x0001)    // Assert ALERT/RDY after two conversions
#define ADS1115_CQUE_4CONV          (0x0002)    // Assert ALERT/RDY after four conversions
#define ADS1115_CQUE_NONE           (0x0003)    // Disable the comparator and put ALERT/RDY in high state (default)

#define ADS1115_DEFAULT_CONFIG_REG  (0x8583)    // Config register value after reset

//***************************************************************************

static const uint8_t ads1115_i2c_id = 0;
static const uint8_t general_i2c_addr = 0x00;
static const uint8_t ads1115_i2c_reset = 0x06;

typedef struct {
    uint8_t addr;
    uint8_t chip_id;
    uint16_t gain;
    uint16_t samples_value; // sample per second
    uint16_t samples;       // register value
    uint16_t comp;
    uint16_t mode;
    uint16_t threshold_low;
    uint16_t threshold_hi;
    uint16_t config;
    int timer_ref;
    os_timer_t timer;
} ads_ctrl_struct_t;

static ads_ctrl_struct_t * ads_ctrl_table[4];

static int ads1115_lua_readoutdone(void * param);

static uint8_t write_reg(uint8_t ads_addr, uint8_t reg, uint16_t config) {
    platform_i2c_send_start(ads1115_i2c_id);
    platform_i2c_send_address(ads1115_i2c_id, ads_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(ads1115_i2c_id, reg);
    platform_i2c_send_byte(ads1115_i2c_id, (uint8_t)(config >> 8));
    platform_i2c_send_byte(ads1115_i2c_id, (uint8_t)(config & 0xFF));
    platform_i2c_send_stop(ads1115_i2c_id);
}

static uint16_t read_reg(uint8_t ads_addr, uint8_t reg) {
    platform_i2c_send_start(ads1115_i2c_id);
    platform_i2c_send_address(ads1115_i2c_id, ads_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(ads1115_i2c_id, reg);
    platform_i2c_send_stop(ads1115_i2c_id);
    platform_i2c_send_start(ads1115_i2c_id);
    platform_i2c_send_address(ads1115_i2c_id, ads_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    uint16_t buf = (platform_i2c_recv_byte(ads1115_i2c_id, 1) << 8);
    buf += platform_i2c_recv_byte(ads1115_i2c_id, 0);
    platform_i2c_send_stop(ads1115_i2c_id);
    return buf;
}

// convert ADC value to voltage corresponding to PGA settings
static double get_volt(uint16_t gain, uint16_t value) {

    double volt = 0;

    switch (gain) {
        case (ADS1115_PGA_6_144V):
            volt = (int16_t)value * 0.1875;
            break;
        case (ADS1115_PGA_4_096V):
            volt = (int16_t)value * 0.125;
            break;
        case (ADS1115_PGA_2_048V):
            volt = (int16_t)value * 0.0625;
            break;
        case (ADS1115_PGA_1_024V):
            volt = (int16_t)value * 0.03125;
            break;
        case (ADS1115_PGA_0_512V):
            volt = (int16_t)value * 0.015625;
            break;
        case (ADS1115_PGA_0_256V):
            volt = (int16_t)value * 0.0078125;
            break;
    }

    return volt;
}

// convert threshold in volt to ADC value corresponding to PGA settings
static uint8_t get_value(uint16_t gain, uint16_t channel, int16_t *volt) {

    switch (gain) {
        case (ADS1115_PGA_6_144V):
            if ((*volt >= 6144) || (*volt < -6144) || ((*volt < 0) && (channel >> 14))) return 1;
            *volt = *volt / 0.1875;
            break;
        case (ADS1115_PGA_4_096V):
            if ((*volt >= 4096) || (*volt < -4096) || ((*volt < 0) && (channel >> 14))) return 1;
            *volt = *volt / 0.125;
            break;
        case (ADS1115_PGA_2_048V):
            if ((*volt >= 2048) || (*volt < -2048) || ((*volt < 0) && (channel >> 14))) return 1;
            *volt = *volt / 0.0625;
            break;
        case (ADS1115_PGA_1_024V):
            if ((*volt >= 1024) || (*volt < -1024) || ((*volt < 0) && (channel >> 14))) return 1;
            *volt = *volt / 0.03125;
            break;
        case (ADS1115_PGA_0_512V):
            if ((*volt >= 512) || (*volt < -512) || ((*volt < 0) && (channel >> 14))) return 1;
            *volt = *volt / 0.015625;
            break;
        case (ADS1115_PGA_0_256V):
            if ((*volt >= 256) || (*volt < -256) || ((*volt < 0) && (channel >> 14))) return 1;
            *volt = *volt / 0.0078125;
            break;
    }

    return 0;
}


// Soft reset of all devices
// Lua:     ads1115.reset()
static int ads1115_lua_reset(lua_State *L) {
    platform_i2c_send_start(ads1115_i2c_id);
    platform_i2c_send_address(ads1115_i2c_id, general_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(ads1115_i2c_id, ads1115_i2c_reset);
    platform_i2c_send_stop(ads1115_i2c_id);
    return 0;
}

// Initializes ADC
// Lua:     ads1115.setup(ADDRESS, CHIP_ID)
static int ads1115_lua_setup(lua_State *L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        return luaL_error(L, "wrong arg range");
    }
    uint8_t i2c_addr = luaL_checkinteger(L, 1);
    if (!IS_I2C_ADDR_VALID(i2c_addr)) {
        return luaL_error(L, "Invalid argument: adddress");
    }
    uint8_t chip_id = luaL_checkinteger(L, 2);
    if (chip_id != ADS1115_ADS1015 && chip_id != ADS1115_ADS1115) {
        return luaL_error(L, "Invalid argument: chip_id");
    }
    // check for device on i2c bus
    if (read_reg(i2c_addr, ADS1115_POINTER_CONFIG) != ADS1115_DEFAULT_CONFIG_REG) {
        return luaL_error(L, "found no device");
    }
    int idx = i2c_addr & 0x03; // 48->0 49->1 4A->2 4B->3
    ads_ctrl_struct_t * ads_ctrl = ads_ctrl_table[idx];
    if (NULL == ads_ctrl) {
        ads_ctrl = c_malloc(sizeof(ads_ctrl_struct_t));
        if (NULL == ads_ctrl) {
            return luaL_error(lua_getstate(), "ads1115 malloc: out of memory");
        }
        ads_ctrl_table[idx] = ads_ctrl;
    }
    memset(ads_ctrl, 0, sizeof(ads_ctrl_struct_t));
    ads_ctrl->chip_id = chip_id;
    ads_ctrl->addr = i2c_addr;
    ads_ctrl->gain = ADS1115_PGA_6_144V;
    ads_ctrl->samples = ADS1115_DR_128SPS;
    ads_ctrl->samples_value = chip_id == ADS1115_ADS1115 ? 128 : 1600;
    ads_ctrl->comp = ADS1115_CQUE_NONE;
    ads_ctrl->mode = ADS1115_MODE_SINGLE;
    ads_ctrl->threshold_low = 0x8000;
    ads_ctrl->threshold_hi = 0x7FFF;
    ads_ctrl->config = 0x8583;
    return 0;
}

// Change ADC settings
// Lua:     ads1115.setting(ADDR,GAIN,SAMPLES,CHANNEL,MODE[,CONVERSION_RDY][,COMPARATOR,THRESHOLD_LOW,THRESHOLD_HI[,COMP_MODE])
static int ads1115_lua_setting(lua_State *L) {

    // check variables
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 5)) {
        return luaL_error(L, "wrong arg range");
    }

    uint8_t addr = luaL_checkinteger(L, 1);
    if (!IS_I2C_ADDR_VALID(addr)) {
        return luaL_error(L, "Invalid argument: adddress");
    }
    ads_ctrl_struct_t * ads_ctrl = ads_ctrl_table[addr & 3];
    if (NULL == ads_ctrl) {
        return luaL_error(L, "Unitialized device");
    }

    uint16_t gain = luaL_checkinteger(L, 2);
    if (!((gain == ADS1115_PGA_6_144V) || (gain == ADS1115_PGA_4_096V) || (gain == ADS1115_PGA_2_048V) || (gain == ADS1115_PGA_1_024V) || (gain == ADS1115_PGA_0_512V) || (gain == ADS1115_PGA_0_256V))) {
        return luaL_error(L, "Invalid argument: gain");
    }
    ads_ctrl->gain = gain;

    uint16_t samples_value = luaL_checkinteger(L, 3);
    uint16_t samples = 0;
    if (ads_ctrl->chip_id == ADS1115_ADS1115) {
        switch(samples_value) {
            case ADS1115_DR_8SPS:
                samples = 0;
                break;
            case ADS1115_DR_16SPS:
                samples = 0x20;
                break;
            case ADS1115_DR_32SPS:
                samples = 0x40;
                break;
            case ADS1115_DR_64SPS:
                samples = 0x60;
                break;
            case ADS1115_DR_128SPS: // default
                samples = 0x80;
                break;
            case ADS1115_DR_250SPS:
                samples = 0xA0;
                break;
            case ADS1115_DR_475SPS:
                samples = 0xC0;
                break;
            case ADS1115_DR_860SPS:
                samples = 0xE0;
                break;
            default:
                return luaL_error(L, "Invalid argument: samples");
        }
    } else { // ADS1115_ADS1015
        switch(samples_value) {
            case ADS1115_DR_128SPS:
                samples = 0;
                break;
            case ADS1115_DR_250SPS:
                samples = 0x20;
                break;
            case ADS1115_DR_490SPS:
                samples = 0x40;
                break;
            case ADS1115_DR_920SPS:
                samples = 0x60;
                break;
            case ADS1115_DR_1600SPS: // default
                samples = 0x80;
                break;
            case ADS1115_DR_2400SPS:
                samples = 0xA0;
                break;
            case ADS1115_DR_3300SPS:
                samples = 0xC0;
                break;
            default:
                return luaL_error(L, "Invalid argument: samples");
        }
    }
    ads_ctrl->samples = samples;
    ads_ctrl->samples_value = samples_value;

    uint16_t channel = luaL_checkinteger(L, 4);
    if (!(IS_CHANNEL_VALID(channel))) {
        return luaL_error(L, "Invalid argument: channel");
    }

    uint16_t mode = luaL_checkinteger(L, 5);
    if (!((mode == ADS1115_MODE_SINGLE) || (mode == ADS1115_MODE_CONTIN))) {
        return luaL_error(L, "Invalid argument: mode");
    }
    ads_ctrl->mode = mode;
    uint16_t os = mode == ADS1115_MODE_SINGLE ? ADS1115_OS_SINGLE : ADS1115_OS_NON;

    uint16_t comp = ADS1115_CQUE_NONE;
    uint16_t comparator_mode = ADS1115_CMODE_TRAD;
    // Parse optional parameters
    if (lua_isnumber(L, 6)) {
        // conversion ready mode
        comp = luaL_checkinteger(L, 6);
        if (!((comp == ADS1115_CQUE_1CONV) || (comp == ADS1115_CQUE_2CONV) || (comp == ADS1115_CQUE_4CONV))) {
            return luaL_error(L, "Invalid argument: conversion ready/comparator mode");
        }
        uint16_t threshold_low = 0x7FFF;
        uint16_t threshold_hi = 0x8000;
        if (lua_isnumber(L, 7) && lua_isnumber(L, 8)) {
            threshold_low = luaL_checkinteger(L, 7);
            threshold_hi = luaL_checkinteger(L, 8);
            if ((int16_t)threshold_low > (int16_t)threshold_hi) {
                return luaL_error(L, "Invalid argument: threshold_low > threshold_hi");
            }
            if (get_value(gain, channel, &threshold_low)) {
                return luaL_error(L, "Invalid argument: threshold_low");
            }
            if (get_value(gain, channel, &threshold_hi)) {
                return luaL_error(L, "Invalid argument: threshold_hi");
            }
            if (lua_isnumber(L, 9)) {
                comparator_mode = luaL_checkinteger(L, 9);
                if (comparator_mode != ADS1115_CMODE_WINDOW && comparator_mode != ADS1115_CMODE_TRAD) {
                    return luaL_error(L, "Invalid argument: comparator_mode");
                }
            }
        }
        ads_ctrl->threshold_low = threshold_low;
        ads_ctrl->threshold_hi = threshold_hi;
        NODE_DBG("ads1115 low: %04x\n", threshold_low);
        NODE_DBG("ads1115 hi : %04x\n", threshold_hi);
        write_reg(addr, ADS1115_POINTER_THRESH_LOW, threshold_low);
        write_reg(addr, ADS1115_POINTER_THRESH_HI, threshold_hi);
    }
    ads_ctrl->comp = comp;

    uint16_t config = (os | channel | gain | mode | samples | comparator_mode | ADS1115_CPOL_ACTVLOW | ADS1115_CLAT_NONLAT | comp);
    ads_ctrl->config = config;

    NODE_DBG("ads1115 config: %04x\n", ads_ctrl->config);
    write_reg(addr, ADS1115_POINTER_CONFIG, config);
    return 0;
}

// Read the conversion register from the ADC
// Lua:     ads1115.startread(function(volt, voltdec, adc) print(volt,voltdec,adc) end)
static int ads1115_lua_startread(lua_State *L) {
    // check variables
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "wrong arg range");
    }
    uint8_t addr = luaL_checkinteger(L, 1);

    if (!IS_I2C_ADDR_VALID(addr)) {
        return luaL_error(L, "Invalid argument: adddress");
    }
    ads_ctrl_struct_t * ads_ctrl = ads_ctrl_table[addr & 3];
    if (NULL == ads_ctrl) {
        return luaL_error(L, "Unitialized");
    }

    if (((ads_ctrl->comp == ADS1115_CQUE_1CONV) ||
         (ads_ctrl->comp == ADS1115_CQUE_2CONV) ||
         (ads_ctrl->comp == ADS1115_CQUE_4CONV)) &&
        (ads_ctrl->threshold_low == 0x7FFF) &&
        (ads_ctrl->threshold_hi == 0x8000)) {
        int32_t now = 0;
        if (ads_ctrl->mode == ADS1115_MODE_SINGLE) {
            NODE_DBG("ads1115 trigger config: %04x", ads_ctrl->config);
            now = 0x7FFFFFFF & system_get_time();
            write_reg(addr, ADS1115_POINTER_CONFIG, ads_ctrl->config);
        }

        lua_pushinteger(L, now);
        return 1;
    } else {

        luaL_argcheck(L, (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION), 2, "Must be function");
        lua_pushvalue(L, 2);
        ads_ctrl->timer_ref = luaL_ref(L, LUA_REGISTRYINDEX);

        if (ads_ctrl->mode == ADS1115_MODE_SINGLE) {
            write_reg(addr, ADS1115_POINTER_CONFIG, ads_ctrl->config);
        }

        // Start a timer to wait until ADC conversion is done
        os_timer_disarm(&ads_ctrl->timer);
        os_timer_setfn(&ads_ctrl->timer, (os_timer_func_t *)ads1115_lua_readoutdone, (void *)ads_ctrl);

        int msec = 1; // ADS1115_DR_1600SPS, ADS1115_DR_2400SPS, ADS1115_DR_3300SPS
        switch (ads_ctrl->samples_value) {
            case ADS1115_DR_8SPS:
                msec = 150;
                break;
            case ADS1115_DR_16SPS:
                msec = 75;
                break;
            case ADS1115_DR_32SPS:
                msec = 35;
                break;
            case ADS1115_DR_64SPS:
                msec = 20;
                break;
            case ADS1115_DR_128SPS:
                msec = 10;
                break;
            case ADS1115_DR_250SPS:
                msec = 5;
                break;
            case ADS1115_DR_475SPS:
            case ADS1115_DR_490SPS:
                msec = 3;
                break;
            case ADS1115_DR_860SPS:
            case ADS1115_DR_920SPS:
                msec = 2;
        }
        os_timer_arm(&ads_ctrl->timer, msec, 0);
        return 0;
    }
}

// adc conversion timer callback
static int ads1115_lua_readoutdone(void * param) {
    ads_ctrl_struct_t * ads_ctrl = (ads_ctrl_struct_t *)param;

    uint16_t ads1115_conversion = read_reg(ads_ctrl->addr, ADS1115_POINTER_CONVERSION);
    double ads1115_volt = get_volt(ads_ctrl->gain, ads1115_conversion);
    int ads1115_voltdec = (int)((ads1115_volt - (int)ads1115_volt) * 1000);
    ads1115_voltdec = ads1115_voltdec > 0 ? ads1115_voltdec : 0 - ads1115_voltdec;

    lua_State *L = lua_getstate();
    os_timer_disarm (&ads_ctrl->timer);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ads_ctrl->timer_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, ads_ctrl->timer_ref);
    ads_ctrl->timer_ref = LUA_NOREF;

    lua_pushnumber(L, ads1115_volt);
    lua_pushinteger(L, ads1115_voltdec);
    lua_pushinteger(L, ads1115_conversion);

    lua_call (L, 3, 0);
}

// Read the conversion register from the ADC
// Lua:     volt,voltdec,adc = ads1115.read()
static int ads1115_lua_read(lua_State *L) {
    // check variables
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "wrong arg range");
    }
    uint8_t addr = luaL_checkinteger(L, 1);
    if (!IS_I2C_ADDR_VALID(addr)) {
        return luaL_error(L, "Invalid argument: adddress");
    }
    ads_ctrl_struct_t * ads_ctrl = ads_ctrl_table[addr & 3];
    if (NULL == ads_ctrl) {
        return luaL_error(L, "Uninitialized");
    }

    uint16_t ads1115_conversion = read_reg(addr, ADS1115_POINTER_CONVERSION);
    double ads1115_volt = get_volt(ads_ctrl->gain, ads1115_conversion);
    int ads1115_voltdec = (int)((ads1115_volt - (int)ads1115_volt) * 1000);
    ads1115_voltdec = ads1115_voltdec>0?ads1115_voltdec:0-ads1115_voltdec;

    lua_pushnumber(L, ads1115_volt);
    lua_pushinteger(L, ads1115_voltdec);
    lua_pushinteger(L, ads1115_conversion);

    return 3;
}

static const LUA_REG_TYPE ads1115_map[] = {
    {   LSTRKEY( "reset" ),         LFUNCVAL(ads1115_lua_reset)     },
    {   LSTRKEY( "setup" ),         LFUNCVAL(ads1115_lua_setup)     },
    {   LSTRKEY( "setting" ),       LFUNCVAL(ads1115_lua_setting)   },
    {   LSTRKEY( "startread" ),     LFUNCVAL(ads1115_lua_startread) },
    {   LSTRKEY( "read" ),          LFUNCVAL(ads1115_lua_read)      },
    {   LSTRKEY( "ADDR_GND" ),      LNUMVAL(ADS1115_I2C_ADDR_GND)   },
    {   LSTRKEY( "ADDR_VDD" ),      LNUMVAL(ADS1115_I2C_ADDR_VDD)   },
    {   LSTRKEY( "ADDR_SDA" ),      LNUMVAL(ADS1115_I2C_ADDR_SDA)   },
    {   LSTRKEY( "ADDR_SCL" ),      LNUMVAL(ADS1115_I2C_ADDR_SCL)   },
    {   LSTRKEY( "SINGLE_SHOT" ),   LNUMVAL(ADS1115_MODE_SINGLE)    },
    {   LSTRKEY( "CONTINUOUS" ),    LNUMVAL(ADS1115_MODE_CONTIN)    },
    {   LSTRKEY( "DIFF_0_1" ),      LNUMVAL(ADS1115_MUX_DIFF_0_1)   },
    {   LSTRKEY( "DIFF_0_3" ),      LNUMVAL(ADS1115_MUX_DIFF_0_3)   },
    {   LSTRKEY( "DIFF_1_3" ),      LNUMVAL(ADS1115_MUX_DIFF_1_3)   },
    {   LSTRKEY( "DIFF_2_3" ),      LNUMVAL(ADS1115_MUX_DIFF_2_3)   },
    {   LSTRKEY( "SINGLE_0" ),      LNUMVAL(ADS1115_MUX_SINGLE_0)   },
    {   LSTRKEY( "SINGLE_1" ),      LNUMVAL(ADS1115_MUX_SINGLE_1)   },
    {   LSTRKEY( "SINGLE_2" ),      LNUMVAL(ADS1115_MUX_SINGLE_2)   },
    {   LSTRKEY( "SINGLE_3" ),      LNUMVAL(ADS1115_MUX_SINGLE_3)   },
    {   LSTRKEY( "GAIN_6_144V" ),   LNUMVAL(ADS1115_PGA_6_144V)     },
    {   LSTRKEY( "GAIN_4_096V" ),   LNUMVAL(ADS1115_PGA_4_096V)     },
    {   LSTRKEY( "GAIN_2_048V" ),   LNUMVAL(ADS1115_PGA_2_048V)     },
    {   LSTRKEY( "GAIN_1_024V" ),   LNUMVAL(ADS1115_PGA_1_024V)     },
    {   LSTRKEY( "GAIN_0_512V" ),   LNUMVAL(ADS1115_PGA_0_512V)     },
    {   LSTRKEY( "GAIN_0_256V" ),   LNUMVAL(ADS1115_PGA_0_256V)     },
    {   LSTRKEY( "DR_8SPS" ),       LNUMVAL(ADS1115_DR_8SPS)        },
    {   LSTRKEY( "DR_16SPS" ),      LNUMVAL(ADS1115_DR_16SPS)       },
    {   LSTRKEY( "DR_32SPS" ),      LNUMVAL(ADS1115_DR_32SPS)       },
    {   LSTRKEY( "DR_64SPS" ),      LNUMVAL(ADS1115_DR_64SPS)       },
    {   LSTRKEY( "DR_128SPS" ),     LNUMVAL(ADS1115_DR_128SPS)      },
    {   LSTRKEY( "DR_250SPS" ),     LNUMVAL(ADS1115_DR_250SPS)      },
    {   LSTRKEY( "DR_475SPS" ),     LNUMVAL(ADS1115_DR_475SPS)      },
    {   LSTRKEY( "DR_490SPS" ),     LNUMVAL(ADS1115_DR_490SPS)      },
    {   LSTRKEY( "DR_860SPS" ),     LNUMVAL(ADS1115_DR_860SPS)      },
    {   LSTRKEY( "DR_920SPS" ),     LNUMVAL(ADS1115_DR_920SPS)      },
    {   LSTRKEY( "DR_1600SPS" ),    LNUMVAL(ADS1115_DR_1600SPS)     },
    {   LSTRKEY( "DR_2400SPS" ),    LNUMVAL(ADS1115_DR_2400SPS)     },
    {   LSTRKEY( "DR_3300SPS" ),    LNUMVAL(ADS1115_DR_3300SPS)     },
    {   LSTRKEY( "CONV_RDY_1" ),    LNUMVAL(ADS1115_CQUE_1CONV)     },
    {   LSTRKEY( "CONV_RDY_2" ),    LNUMVAL(ADS1115_CQUE_2CONV)     },
    {   LSTRKEY( "CONV_RDY_4" ),    LNUMVAL(ADS1115_CQUE_4CONV)     },
    {   LSTRKEY( "COMP_1CONV" ),    LNUMVAL(ADS1115_CQUE_1CONV)     },
    {   LSTRKEY( "COMP_2CONV" ),    LNUMVAL(ADS1115_CQUE_2CONV)     },
    {   LSTRKEY( "COMP_4CONV" ),    LNUMVAL(ADS1115_CQUE_4CONV)     },
    {   LSTRKEY( "ADS1015"),        LNUMVAL(ADS1115_ADS1015)        },
    {   LSTRKEY( "ADS1115"),        LNUMVAL(ADS1115_ADS1115)        },
    {   LSTRKEY( "CMODE_TRAD"),     LNUMVAL(ADS1115_CMODE_TRAD)     },
    {   LSTRKEY( "CMODE_WINDOW"),   LNUMVAL(ADS1115_CMODE_WINDOW)   },
    {   LNILKEY, LNILVAL                                            }
};

NODEMCU_MODULE(ADS1115, "ads1115", ads1115_map, NULL);
