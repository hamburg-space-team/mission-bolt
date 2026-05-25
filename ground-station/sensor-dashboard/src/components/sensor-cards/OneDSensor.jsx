import OneDChart from "@/components/charts/OneDChart";

import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";

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

const OneDSensorCard = ({
  sensorType,
  newValue,
  status = "offline",
  color = "#c95318",
  unit = "",
}) => {
  const currentStatus = statusConfig[status] ?? statusConfig.offline;

  return (
    <Card className="flex flex-col">
      <CardHeader className="flex flex-row items-center justify-between">
        <CardTitle className="text-sm font-medium leading-none">
          {sensorType}
        </CardTitle>
        <div className="flex items-center gap-2 text-xs font-medium text-muted-foreground">
          <span>{currentStatus.label}</span>
          <div
            className={`size-2.5 shrink-0 rounded-full ${currentStatus.color}`}
            aria-label={currentStatus.label}
            title={currentStatus.label}
          />
        </div>
      </CardHeader>
      <CardContent>
        <div>
          <OneDChart newValue={newValue} lineColor={color} unit={unit} />
        </div>
      </CardContent>
    </Card>
  );
};

export default OneDSensorCard;
