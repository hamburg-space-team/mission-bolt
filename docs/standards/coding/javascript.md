# JavaScript / TypeScript Coding Standard (Frontend)

## Versions

- Node **20+**
- Package manager: **`bun`** (`npm` works as a fallback)
- React **19**, Vite **8**, ESLint **10**

## Style

- ESLint config in [`eslint.config.js`](../../../ground-station/sensor-dashboard/eslint.config.js)
  enforces React hooks rules and basic JS standards.
- Prettier integrated through ESLint.
- 100-character line length.

Run before committing:

```bash
bun run lint
```

## References

- [Running the ground station](../../guides/running-ground-station.md)
- [`sensor-dashboard/package.json`](../../../ground-station/sensor-dashboard/package.json)
- [`eslint.config.js`](../../../ground-station/sensor-dashboard/eslint.config.js)
