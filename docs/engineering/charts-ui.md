# Charts UI

The `/charts` route presents retained sensor history parsed from binary log
files. It should stay optimized for comparison first: the user is usually
looking for correlation between CO2, temperature, humidity, alarms, and log
events rather than inspecting one isolated sensor.

## Environment History Module

The environment history view intentionally uses one shared chart card for CO2,
temperature, and humidity:

- one shared X axis keeps all readings aligned by timestamp;
- CO2, temperature, and humidity are drawn in separate sensor tracks, because
  ppm, Celsius, and percent cannot share one raw axis without becoming
  misleading;
- the metric tiles and hover readout always show the real raw values and units;
- small changes are not stretched to the full track height. Each sensor has a
  minimum meaningful plot range so flat humidity or temperature data remains
  visually calm instead of looking noisy;
- large time gaps insert null plot points so the chart does not imply continuous
  data collection where the retained log has no samples.

The transformation lives in
`interface/src/routes/charts/components/charts/environmentHistoryModel.ts`.
Keep that file framework-independent so the projection and stats can be
covered by Vitest without a browser or canvas.

## Validation

For chart UI changes, run from `interface/`:

```bash
npm run check
npm run test:run -- src/routes/charts
npm run build
```

When changing layout, also capture responsive screenshots for at least mobile,
tablet, and desktop widths. The chart should remain a single module, with the
metric tiles wrapping cleanly and the tooltip staying inside the chart frame.
