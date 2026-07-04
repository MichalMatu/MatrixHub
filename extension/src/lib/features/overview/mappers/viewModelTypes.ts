export interface DetailItem {
  id: string;
  label: string;
  value: string;
}

export interface OverviewCardViewModel {
  details: DetailItem[];
  metrics: OverviewMetricViewModel[];
}

export interface DeviceStatusViewModel {
  items: DetailItem[];
}

export interface DeviceSettingsViewModel {
  deviceItems: DetailItem[];
  statusItems: DetailItem[];
}

export type OverviewMetricId = "co2" | "temperature" | "humidity";

export interface OverviewMetricViewModel {
  id: OverviewMetricId;
  label: string;
  value: number;
  history: number[];
  timestamps: number[];
  chartColor: string;
  domainMin?: number;
  domainMax?: number;
}
