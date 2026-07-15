// @generated from interfaces/tools/generated/schema.json - do not edit.
// Regenerate with: npm run gen:commands  (or tools/schemagen/run-schemagen.sh upstream).
export const UPLINK_COMMANDS = [
  { id: "reset_tick", label: "Reset Tick", dangerous: true, desc: "reset the 25 Hz tick counter to 0" },
  { id: "start_experiment", label: "Start Experiment", dangerous: false, desc: "arm the experiment sequence" },
  { id: "activate_camera", label: "Activate Camera", dangerous: false, desc: "power the onboard camera" },
  { id: "full_system_test", label: "Full System Test", dangerous: true, desc: "run the built-in self test" },
] as const;

// On-wire bytes per packet type (payload + 12 B header + 2 B CRC), by count name.
export const PACKET_WIRE_BYTES: Record<string, number> = {
  "btc_env": 25,
  "btc_status": 24,
  "btc_imu": 26,
  "exp1_spectrum": 58,
  "exp1_env": 25,
  "exp2_env": 25,
  "exp3_env": 25,
  "exp1_status": 22,
  "exp2_status": 22,
  "exp2_ber": 30,
  "exp3_stack_a": 50,
  "exp3_stack_b": 52,
  "exp3_status": 27,
  "exp3_imu": 26,
  "gap_marker": 19,
  "fault": 27,
  "boot": 18,
  "cmd_ack": 17,
  "timing": 27,
};
