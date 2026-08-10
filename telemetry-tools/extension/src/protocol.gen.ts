// @generated from interfaces/tools/generated/schema.json - do not edit.
// Regenerate with: npm run gen:commands  (or tools/schemagen/run-schemagen.sh upstream).
export const UPLINK_COMMANDS = [
  { id: "reset_tick", label: "Reset Tick", dangerous: true, desc: "reset the 25 Hz tick counter to 0" },
  { id: "start_experiment", label: "Start Experiment", dangerous: false, desc: "arm the experiment sequence" },
  { id: "activate_camera", label: "Activate Camera", dangerous: false, desc: "power the onboard camera" },
  { id: "stop_experiment", label: "Stop Experiment", dangerous: true, desc: "stop the experiment sequence. Falls back to test sequence" },
  { id: "full_system_test", label: "Full System Test", dangerous: true, desc: "run the built-in self test" },
] as const;

// On-wire bytes per packet type (payload + 12 B header + 2 B CRC), by count name.
export const PACKET_WIRE_BYTES: Record<string, number> = {
  "btc_env": 25,
  "btc_status": 25,
  "btc_imu": 26,
  "exp1_spectrum": 58,
  "exp1_env": 25,
  "exp2_env": 25,
  "exp3_env": 25,
  "exp1_status": 23,
  "exp2_status": 23,
  "exp2_ber": 30,
  "exp3_stack_a": 50,
  "exp3_stack_b": 52,
  "exp3_status": 28,
  "exp3_imu": 26,
  "gap_marker": 19,
  "fault": 27,
  "boot": 18,
  "cmd_ack": 17,
  "btc_timing": 26,
  "exp1_timing": 26,
  "exp2_timing": 26,
  "exp3_timing": 26,
  "btc_test": 21,
  "exp1_test": 21,
  "exp2_test": 21,
  "exp3_test": 21,
};

// Per-node self-test step names, indexed by test_id (bolt/wire/selftest.hpp).
export const SELF_TEST_STEPS: Record<string, string[]> = {
  btc: ["TMP117 WHO_AM_I","TMP117 read","MS5611 PROM CRC","ICM-42686 WHO_AM_I","ICM-42686 read","SD mounted"],
  exp1: ["TMP117 WHO_AM_I","TMP117 read","MS5611 PROM CRC","AS7265x HW version","LP5810C configured","LP5810D configured","Spectrum dark","Spectrum red","Spectrum green","Spectrum blue","Spectrum white","Spectrum IR 940nm","Spectrum UV 400nm","SD mounted"],
  exp2: ["TMP117 WHO_AM_I","TMP117 read","MS5611 PROM CRC","SD mounted"],
  exp3: ["TMP117 WHO_AM_I","TMP117 read","MS5611 PROM CRC","ICM-42686 WHO_AM_I","ICM-42686 read","SD mounted"],
};
