export const ERROR_CODES: Record<number, string> = {
  1: "BUS_ERROR",
  2: "TIMEOUT",
  3: "BAD_ARGUMENT",
  4: "DISABLED",
  5: "PROTOCOL_ERROR",
  6: "IO_ERROR",
  7: "OUTPUT_TOO_LARGE",
};

export const FAULT_CODES: Record<number, string> = {
  1: "I²C bus",
  2: "MS5611 barometer",
  3: "TMP117 temperature",
  4: "SD / storage",
  5: "CAN bus",
  6: "RS-422 downlink",
  7: "ICM-42686 IMU",
  8: "AS7265X spectrometer",
  9: "RGB LED driver (LP5810C)",
  10: "UV/IR LED driver (LP5810D)",
};

export const FAULT_NODES: Record<number, string> = {
  1: "all",
  2: "all",
  3: "all",
  4: "all",
  5: "EXPs",
  6: "BTC",
  7: "BTC/EXP3",
  8: "EXP1",
  9: "EXP1",
  10: "EXP1",
};

export const GAP_NODE_HINT: Record<string, string> = {
  lifi_timeout: "EXP3",
  can_crc_fail: "BTC←EXP",
  no_data: "BTC/EXP",
  sensor_failed: "device",
};

export const STEP_CODES: Record<number, string> = {
  0: "NONE", 16: "I2C_INIT", 17: "I2C_RESET", 18: "I2C_WRITE", 19: "I2C_READ",
  20: "I2C_WRITE_READ", 21: "I2C_WAIT_COMPLETE", 32: "BARO_RESET", 33: "BARO_PROM_READ",
  34: "BARO_PROM_CRC", 35: "BARO_START_CONV", 36: "BARO_READ_ADC", 37: "BARO_COLLECT",
  38: "BARO_READ", 48: "TMP_INIT", 49: "TMP_ID_CHECK", 50: "TMP_CONFIG", 51: "TMP_READ",
  64: "SPEC_INIT", 65: "SPEC_START_MEAS", 66: "SPEC_SET_INTEGRATION", 67: "SPEC_WAIT_STATUS",
  68: "SPEC_VREG_WRITE", 69: "SPEC_VREG_READ", 70: "SPEC_DEV_SEL", 71: "SPEC_READ_DIES",
  80: "LED_INIT", 81: "LED_CONFIGURE", 82: "LED_ENABLE_CHIP", 83: "LED_SET_CHANNELS",
  84: "LED_APPLY", 85: "LED_DISABLE_ALL", 86: "LED_RECOVER", 96: "IMU_WHOAMI",
  97: "IMU_CONFIG_ACCEL", 98: "IMU_CONFIG_GYRO", 99: "IMU_POWER_ON", 100: "IMU_READ",
  101: "IMU_CONFIG_INT", 112: "SD_INIT", 113: "SD_MOUNT", 114: "SD_OPEN", 115: "SD_WRITE",
  116: "SD_FLUSH", 128: "PKT_BUILD", 129: "CAN_TX_RING", 130: "UART_TX_RING",
};

export function stepName(code: number): string {
  return STEP_CODES[code] ?? `0x${code.toString(16)}`;
}

export function errorName(code: number): string {
  return ERROR_CODES[code] ?? `err ${code}`;
}

export function faultName(code: number): string {
  return FAULT_CODES[code] ?? `fault ${code}`;
}

export function faultNode(code: number): string {
  return FAULT_NODES[code] ?? "?";
}

export function gapNode(reason: string): string {
  return GAP_NODE_HINT[reason] ?? "?";
}
