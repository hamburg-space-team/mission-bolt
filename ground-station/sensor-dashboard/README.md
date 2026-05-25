# Sensor Dashboard

React + Vite frontend for live telemetry. Subscribes to the
sensor-api's SSE stream and renders incoming packets.

For the dev flow and overall ground-station picture, see
[`../README.md`](../README.md) and the
[ground-station guide](../../docs/guides/running-ground-station.md).

## Stack

- React 19, Vite 8
- Tailwind 4, shadcn/ui
- ECharts (via `echarts-for-react`) for plots
- ESLint 10

## Run

```bash
bun install
bun run dev          # http://localhost:5173
```

`npm install` / `npm run dev` work the same.

## Build

```bash
bun run build
bun run preview
```

## Lint

```bash
bun run lint
```

## Notes

- Live data arrives over SSE. The connection is opened once at the
  app root and pushed to consumers via context. Components don't
  open their own connections.
- New components: functional, hooks, named export. See
  [docs/standards/coding/javascript.md](../../docs/standards/coding/javascript.md).
- TODO: migrate to TypeScript before flight. New files can be
  `.tsx` already.
- TODO: pick a test runner. Plan is Vitest.
