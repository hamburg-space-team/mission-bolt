import { useEffect, useRef } from "react";
import * as echarts from "echarts/core";
import { LineChart } from "echarts/charts";
import { GridComponent, TooltipComponent, LegendComponent, DataZoomComponent, MarkAreaComponent } from "echarts/components";
import { CanvasRenderer } from "echarts/renderers";

echarts.use([LineChart, GridComponent, TooltipComponent, LegendComponent, DataZoomComponent, MarkAreaComponent, CanvasRenderer]);

export interface Series {
  name: string;
  color: string;
  data: [number, number][]; // [t_seconds, value]
}

const cssVar = (name: string, fallback: string) =>
  getComputedStyle(document.body).getPropertyValue(name).trim() || fallback;

export function TimeSeriesChart({
  series,
  yLabel,
  height = 240,
  cursor,
}: {
  series: Series[];
  yLabel?: string;
  height?: number;
  cursor?: number | null;
}) {
  const host = useRef<HTMLDivElement>(null);
  const chart = useRef<echarts.ECharts | null>(null);

  useEffect(() => {
    if (!host.current) return;
    chart.current = echarts.init(host.current, null, { renderer: "canvas" });
    const onResize = () => chart.current?.resize();
    window.addEventListener("resize", onResize);
    return () => {
      window.removeEventListener("resize", onResize);
      chart.current?.dispose();
      chart.current = null;
    };
  }, []);

  useEffect(() => {
    const fg = cssVar("--vscode-foreground", "#ccc");
    const border = cssVar("--vscode-widget-border", "#8884");
    chart.current?.setOption(
      {
        backgroundColor: "transparent",
        textStyle: { color: fg },
        animation: false,
        grid: { left: 56, right: 16, top: 28, bottom: 64 },
        tooltip: { trigger: "axis" },
        legend: { top: 0, textStyle: { color: fg } },
        xAxis: {
          type: "value",
          name: "t (s)",
          // t is seconds since the board booted, so a session joined late
          // starts at 1800 rather than 0. Without this echarts pads the axis
          // down to zero and squeezes the data into the right-hand edge
          scale: true,
          nameTextStyle: { color: fg },
          axisLine: { lineStyle: { color: border } },
          axisLabel: { color: fg, hideOverlap: true },
          splitLine: { lineStyle: { color: border, opacity: 0.4 } },
        },
        yAxis: {
          type: "value",
          name: yLabel,
          nameTextStyle: { color: fg },
          scale: true,
          axisLine: { lineStyle: { color: border } },
          axisLabel: { color: fg },
          splitLine: { lineStyle: { color: border, opacity: 0.4 } },
        },
        dataZoom: [
          { type: "inside", xAxisIndex: 0 },
          { type: "slider", xAxisIndex: 0, height: 18, bottom: 24, borderColor: border, textStyle: { color: fg } },
        ],
        series: series.map((s) => ({
          name: s.name,
          type: "line",
          showSymbol: false,
          sampling: "lttb",
          large: true,
          lineStyle: { width: 1.3, color: s.color },
          itemStyle: { color: s.color },
          data: s.data,
        })),
      },
      { replaceMerge: ["series"] },
    );
  }, [series, yLabel]);

  // Cheap markLine-only update (merges into series[0], no data re-diff).
  useEffect(() => {
    if (cursor == null) {
      chart.current?.setOption({ series: [{ markLine: { data: [] } }] });
      return;
    }
    chart.current?.setOption({
      series: [
        {
          markLine: {
            silent: true,
            symbol: "none",
            animation: false,
            data: [{ xAxis: cursor }],
            lineStyle: { color: "#e5c07b", width: 1.4 },
            label: { show: false },
          },
        },
      ],
    });
  }, [cursor]);

  return <div ref={host} style={{ width: "100%", height }} />;
}
