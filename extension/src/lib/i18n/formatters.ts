import type { I18nRuntime } from "./runtime";

type TimeUnitKey = "minute" | "hour" | "day";

function roundToFixedDecimal(value: number, fractionDigits: number) {
  return Number(value.toFixed(fractionDigits));
}

function formatShortTimeUnit(
  i18n: I18nRuntime,
  unit: TimeUnitKey,
  count: number,
) {
  return i18n.t(`time.units.${unit}.short`, {
    count,
    value: i18n.formatInteger(count),
  });
}

export function formatCompactElapsedAgo(
  i18n: I18nRuntime,
  timestamp: number,
  now = Date.now(),
) {
  const totalMinutes = Math.floor(Math.max(0, now - timestamp) / 60000);
  if (totalMinutes < 1) {
    return i18n.t("time.lessThanMinuteAgo");
  }
  if (totalMinutes < 60) {
    return i18n.t("time.ago", {
      value: formatShortTimeUnit(i18n, "minute", totalMinutes),
    });
  }

  const hours = Math.floor(totalMinutes / 60);
  const minutes = totalMinutes % 60;
  if (hours < 24) {
    const parts = [formatShortTimeUnit(i18n, "hour", hours)];
    if (minutes > 0) {
      parts.push(formatShortTimeUnit(i18n, "minute", minutes));
    }

    return i18n.t("time.ago", {
      value: parts.join(" "),
    });
  }

  return i18n.t("time.ago", {
    value: formatShortTimeUnit(i18n, "day", Math.floor(hours / 24)),
  });
}

export function formatUptime(i18n: I18nRuntime, totalSeconds: number) {
  const days = Math.floor(totalSeconds / 86400);
  const hours = Math.floor((totalSeconds % 86400) / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);

  if (days > 0) {
    return [
      formatShortTimeUnit(i18n, "day", days),
      formatShortTimeUnit(i18n, "hour", hours),
    ].join(" ");
  }

  if (hours > 0) {
    return [
      formatShortTimeUnit(i18n, "hour", hours),
      formatShortTimeUnit(i18n, "minute", minutes),
    ].join(" ");
  }

  return formatShortTimeUnit(i18n, "minute", minutes);
}

export function formatTemperatureValue(i18n: I18nRuntime, value: number) {
  return `${i18n.formatDecimal(roundToFixedDecimal(value, 1), 1)} C`;
}

export function formatCompactTemperatureValue(
  i18n: I18nRuntime,
  value: number,
) {
  return `${i18n.formatDecimal(roundToFixedDecimal(value, 1), 1)}C`;
}

export function formatHumidityValue(i18n: I18nRuntime, value: number) {
  return `${i18n.formatDecimal(roundToFixedDecimal(value, 1), 1)} %`;
}

export function formatCompactHumidityValue(i18n: I18nRuntime, value: number) {
  return `${i18n.formatInteger(Math.round(value))}%`;
}

export function formatBatteryValue(i18n: I18nRuntime, value: number) {
  return `${i18n.formatInteger(Math.round(value))} %`;
}

export function formatCo2Value(i18n: I18nRuntime, value: number) {
  return `${i18n.formatInteger(Math.round(value))} ppm`;
}

export function formatRssiValue(i18n: I18nRuntime, value: number) {
  return `${i18n.formatInteger(value)} dBm`;
}
