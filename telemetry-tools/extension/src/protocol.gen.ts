// @generated from interfaces/tools/generated/schema.json (CommandOpcode) - do not edit.
// Regenerate with: npm run gen:commands  (or tools/schemagen/run-schemagen.sh upstream).
export const UPLINK_COMMANDS = [
  { id: "reset_tick", label: "Reset Tick", dangerous: true, desc: "reset the 25 Hz tick counter to 0" },
  { id: "start_experiment", label: "Start Experiment", dangerous: false, desc: "arm the experiment sequence" },
  { id: "activate_camera", label: "Activate Camera", dangerous: false, desc: "power the onboard camera" },
  { id: "full_system_test", label: "Full System Test", dangerous: true, desc: "run the built-in self test" },
] as const;
