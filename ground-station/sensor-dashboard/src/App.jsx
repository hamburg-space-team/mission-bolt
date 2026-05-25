import { useState, useEffect } from "react";

import "./index.css";

import { ThemeProvider } from "@/components/ThemeProvider";
import Navbar from "@/components/Navbar";
import { Card } from "@/components/ui/card";
import OneDSensorCard from "@/components/sensor-cards/OneDSensor";

const statusConfig = {
  live: {
    label: "Live",
    color: "bg-emerald-500",
  },
  error: {
    label: "Error",
    color: "bg-red-500",
  },
  offline: {
    label: "Offline",
    color: "bg-zinc-600",
  },
};

const MAX_LOG_LINES = 5000;
const logLevelClasses = {
  info: "text-cyan-300",
  warning: "text-amber-300",
  error: "text-red-300",
};

function appendLogLine(setter, text, level = "info", source = "Server") {
  setter((prev) => {
    const next = [
      ...prev,
      {
        timestamp: new Date().toLocaleTimeString(),
        source,
        level,
        text,
      },
    ];
    if (next.length <= MAX_LOG_LINES) return next;
    return next.slice(next.length - MAX_LOG_LINES);
  });
}

function App() {
  const [sensorValue, setSensorValue] = useState("Connecting...");
  const [status, setStatus] = useState("offline");
  const [activeTab, setActiveTab] = useState("BTC");
  const [logLines, setLogLines] = useState([]);
  const currentStatus = statusConfig[status] ?? statusConfig.offline;

  useEffect(() => {
    // Connect to Go server-sent events
    const eventSource = new EventSource("http://localhost:8000/events");

    eventSource.onopen = () => {
      setStatus("live");
      console.log("Connected to SSE");
      appendLogLine(setLogLines, "Connected to SSE stream", "info", "Server");
    };

    eventSource.onerror = () => {
      setStatus("error");
      appendLogLine(setLogLines, "SSE connection error", "error", "Server");
    };

    eventSource.onmessage = (event) => {
      // Update our state with the new value
      setSensorValue(event.data);
    };

    return () => {
      eventSource.close();
      setStatus("offline");
      setSensorValue("---");
    };
  }, []);

  return (
    <ThemeProvider defaultTheme="dark" storageKey="vite-ui-theme">
      <div className="min-h-screen bg-background text-foreground">
        <div className="grid grid-cols-1 items-start xl:grid-cols-[minmax(0,1fr)_24rem]">
          <div className="min-w-0">
            <Navbar activeTab={activeTab} onTabChange={setActiveTab} />
            <main className="w-full space-y-3 px-4 pb-6 pt-2 sm:px-6 lg:px-10">
              <section
                className={activeTab === "BTC" ? "space-y-3" : "hidden"}
                hidden={activeTab !== "BTC"}
              >
                <Card className="border-border/60 bg-card/80 shadow-sm">
                  <div className="flex items-center justify-between gap-3 px-4 py-3 sm:px-5">
                    <h1 className="text-2xl font-semibold tracking-tight text-foreground">
                      BTC
                    </h1>
                    <div className="flex items-center justify-end gap-2 text-sm font-medium text-muted-foreground">
                      <span>{currentStatus.label}</span>
                      <div
                        className={`size-3 shrink-0 rounded-full ${currentStatus.color}`}
                        aria-label={currentStatus.label}
                        title={currentStatus.label}
                      />
                    </div>
                  </div>
                </Card>
                <div className="grid grid-cols-1 gap-2 md:grid-cols-2 xl:grid-cols-2">
                  <OneDSensorCard
                    sensorType="Temperature"
                    newValue={sensorValue}
                    status={status}
                    color="#ff78e2"
                    unit="°C"
                  />
                  <OneDSensorCard
                    sensorType="Humidity"
                    newValue={sensorValue}
                    status={status}
                    color="#78ffb2"
                    unit="%"
                  />
                  <OneDSensorCard
                    sensorType="Linear Speed"
                    newValue={sensorValue}
                    status={status}
                    color="#29ebff"
                    unit="m/s"
                  />
                  <OneDSensorCard
                    sensorType="Angular Speed"
                    newValue={sensorValue}
                    status={status}
                    color="#fd2a25"
                    unit="°/s"
                  />
                </div>
              </section>

              <section
                className={activeTab === "Experiment 1" ? "block" : "hidden"}
                hidden={activeTab !== "Experiment 1"}
              >
                <Card className="border-border/60 bg-card/80 shadow-sm">
                  <div className="space-y-2 px-4 py-4 sm:px-5">
                    <h2 className="text-lg font-semibold tracking-tight text-foreground">
                      Experiment 1
                    </h2>
                    <p className="text-sm text-muted-foreground">
                      This tab can host a different component tree while keeping
                      the root sensor state alive.
                    </p>
                  </div>
                </Card>
              </section>

              <section
                className={activeTab === "Experiment 2" ? "block" : "hidden"}
                hidden={activeTab !== "Experiment 2"}
              >
                <Card className="border-border/60 bg-card/80 shadow-sm">
                  <div className="space-y-2 px-4 py-4 sm:px-5">
                    <h2 className="text-lg font-semibold tracking-tight text-foreground">
                      Experiment 2
                    </h2>
                    <p className="text-sm text-muted-foreground">
                      The websocket connection and chart histories remain
                      mounted in the app root.
                    </p>
                  </div>
                </Card>
              </section>

              <section
                className={activeTab === "Experiment 3" ? "block" : "hidden"}
                hidden={activeTab !== "Experiment 3"}
              >
                <Card className="border-border/60 bg-card/80 shadow-sm">
                  <div className="space-y-2 px-4 py-4 sm:px-5">
                    <h2 className="text-lg font-semibold tracking-tight text-foreground">
                      Experiment 3
                    </h2>
                    <p className="text-sm text-muted-foreground">
                      Replace this panel with another view without losing sensor
                      data or live updates.
                    </p>
                  </div>
                </Card>
              </section>
            </main>
          </div>

          <aside className="h-[calc(100vh-4rem)] xl:sticky xl:top-0 xl:h-screen xl:border-l xl:border-border/60">
            <div className="h-full p-4 sm:p-5">
              <div className="h-full overflow-y-auto rounded-md border border-border bg-muted/20 p-3 font-mono text-xs leading-relaxed text-foreground">
                {logLines.length === 0 ? (
                  <p className="text-muted-foreground">No logs yet.</p>
                ) : (
                  logLines.map((line, index) => (
                    <p
                      key={`${index}-${line.timestamp}-${line.text}`}
                      className={
                        logLevelClasses[line.level] || "text-foreground"
                      }
                    >
                      [{line.timestamp}] {line.source}: {line.text}
                    </p>
                  ))
                )}
              </div>
            </div>
          </aside>
        </div>
      </div>
    </ThemeProvider>
  );
}

export default App;
