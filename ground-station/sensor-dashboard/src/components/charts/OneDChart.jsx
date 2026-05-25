import ReactECharts from "echarts-for-react";
import { useEffect, useState } from "react";

function hexToRgba(hex, alpha = 1) {
  if (!hex) return `rgba(0,0,0,${alpha})`;
  const h = hex.replace("#", "");
  const normalized =
    h.length === 3
      ? h
          .split("")
          .map((c) => c + c)
          .join("")
      : h;
  const bigint = parseInt(normalized, 16);
  const r = (bigint >> 16) & 255;
  const g = (bigint >> 8) & 255;
  const b = bigint & 255;
  return `rgba(${r}, ${g}, ${b}, ${alpha})`;
}

const OneDChart = ({
  newValue,
  lineColor = "#c95318",
  areaColor,
  unit = "",
}) => {
  const [data, setData] = useState([]);

  useEffect(() => {
    if (newValue === "Connecting..." || newValue === "---") return;

    setData((prevData) => {
      const now = new Date();
      const newDataPoint = [now, parseFloat(newValue)];
      const updatedData = [...prevData, newDataPoint];
      return updatedData;
    });
  }, [newValue]);

  const option = {
    animation: false,
    grid: {
      left: "5%",
      right: "5%",
      top: "5%",
      bottom: "5%",
    },
    sampling: "lttb",
    tooltip: { trigger: "axis" },
    dataZoom: [
      {
        type: "inside",
        realtime: false,
      },
      // {
      //   type: "slider",
      //   realtime: true,
      // },
    ],
    xAxis: {
      type: "time",
      boundaryGap: false,
      splitLine: { show: false },
      axisLabel: {
        color: "#888",
        // Format to show seconds and milliseconds
        // formatter: "{mm}:{ss}.{SSS}",
      },
    },
    yAxis: {
      type: "value",
      axisLabel: {
        color: "#888",
        formatter: (value) => (unit ? `${value} ${unit}` : `${value}`),
      },
    },
    series: [
      {
        name: "Sensor 1",
        type: "line",
        sampling: "lttb",
        showSymbol: false,
        data: data,
        itemStyle: { color: lineColor },
        areaStyle: {
          color: areaColor || hexToRgba(lineColor, 0.18),
        },
      },
    ],
    animation: false, // Disable animations for real-time speed
  };

  return (
    <ReactECharts
      option={option}
      opts={{ renderer: "canvas" }}
      lazyUpdate={true}
    />
  );
};

export default OneDChart;
