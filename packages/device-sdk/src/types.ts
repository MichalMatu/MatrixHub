export type StorageMetricsInfo = {
  backend: string;
  available: boolean;
  mounted: boolean;
  total_bytes: number;
  used_bytes: number;
  free_bytes: number;
  last_error?: string;
};

export type SystemStorageInfo = {
  filesystem: { total_bytes: number; used_bytes: number; free_bytes: number };
  active_backend: string;
  active_path: string;
  active: StorageMetricsInfo;
  littlefs: StorageMetricsInfo;
  sdcard: StorageMetricsInfo;
};

export type SystemInformation = {
  esp_platform: string;
  firmware_version: string;
  firmware_name?: string;
  firmware_built_target?: string;
  firmware_commit: string;
  firmware_dirty: boolean;
  cpu_freq_mhz: number;
  cpu_type: string;
  cpu_rev: number;
  cpu_cores: number;
  sketch_size: number;
  free_sketch_space: number;
  sdk_version: string;
  arduino_version: string;
  flash_chip_size: number;
  flash_chip_speed: number;
  cpu_reset_reason: number;
  max_alloc_heap: number;
  psram_size: number;
  free_psram: number;
  used_psram: number;
  free_heap: number;
  used_heap: number;
  total_heap: number;
  min_free_heap: number;
  core_temp: number;
  fs_total: number;
  fs_used: number;
  lp_sram_used?: number;
  lp_sram_free?: number;
  lp_sram_total?: number;
  uptime: number;
  mac_address?: string;
  compile_date?: string;
  compile_time?: string;
  storage?: SystemStorageInfo;
};

export interface SystemStatus {
  timestamp: number;
  lastUpdate: number;
  wifiStatus: number;
  rssi: number;
  isConnected: boolean;
  isStaConnected?: boolean;
  isApMode?: boolean;
  coreTemp?: number;
}

export const DEFAULT_SYSTEM_STATUS: SystemStatus = {
  timestamp: 0,
  lastUpdate: 0,
  wifiStatus: 0,
  rssi: 0,
  isConnected: false,
  isStaConnected: false,
  isApMode: false,
  coreTemp: 0,
};

export interface SystemStatusWifiDiagnostics {
  connected?: boolean;
  lastDisconnectReason?: number;
  healthy?: boolean;
  state?: string;
  mode?: string;
  rescueApActive?: boolean;
  rescueReason?: string;
  lastRecoveryReason?: string;
  lastIpChangeMs?: number;
  disconnectedSinceMs?: number;
  stableConnectedSinceMs?: number;
  rssi?: number;
  ssid?: string;
  ip?: string;
  savedStaticIp?: string;
  mac?: string;
  channel?: number;
  bssid?: string;
  gateway?: string;
  subnet?: string;
  dns?: string;
}

export interface SystemStatusApDiagnostics {
  active?: boolean;
  mode?: string;
  stationNum?: number;
  ip?: string;
  mac?: string;
}

export interface SystemStatusHttpDiagnostics {
  activeClients?: number;
  peakClients?: number;
  opens?: number;
  closes?: number;
  lastOpenMs?: number;
  lastCloseMs?: number;
  wsActiveClients?: number;
  wsPeakClients?: number;
  wsOpens?: number;
  wsCloses?: number;
  lastWsOpenMs?: number;
  lastWsCloseMs?: number;
  wsForcedRemovals?: number;
  wsQueueDrops?: number;
  lastWsQueueDropMs?: number;
  lastWsQueueDropPayload?: number;
  wsHeapFallbacks?: number;
  lastWsHeapFallbackMs?: number;
  lastWsHeapFallbackPayload?: number;
  maxWsHeapFallbackPayload?: number;
}

export interface SystemStatusForwardingDiagnostics {
  ready?: boolean;
  requiresStaticIp?: boolean;
  savedStaticIpConfigured?: boolean;
  savedStaticIpMatches?: boolean;
  httpsPort?: number;
}

export interface DashboardBleWidgetSummary {
  enabled: boolean;
  sensor_count: number;
}

export interface DashboardShellyWidgetSummary {
  device_count: number;
}

export interface DashboardAlarmsWidgetSummary {
  rule_count: number;
}

export interface DashboardWifiSensingWidgetSummary {
  enabled: boolean;
}

export interface SystemStatusDashboardWidgetsSummary {
  ble: DashboardBleWidgetSummary;
  shelly: DashboardShellyWidgetSummary;
  alarms: DashboardAlarmsWidgetSummary;
  wifi_sensing: DashboardWifiSensingWidgetSummary;
}

export interface SystemStatusSnapshot {
  system_info?: SystemInformation;
  diagnostics?: {
    wifi?: SystemStatusWifiDiagnostics;
    ap?: SystemStatusApDiagnostics;
    http?: SystemStatusHttpDiagnostics;
    forwarding?: SystemStatusForwardingDiagnostics;
  };
  dashboard_widgets?: SystemStatusDashboardWidgetsSummary;
  wifi_ap_mode?: boolean;
  wifi_settings?: {
    connection_mode?: number;
    wifi_networks?: Array<{ ssid: string }>;
  };
  config?: {
    logging?: {
      level?: string;
    };
  };
}

export interface TelemetryHistorySnapshot {
  timestamps?: number[];
  co2?: Array<number | null>;
  temp?: Array<number | null>;
  humid?: Array<number | null>;
}

export interface TelemetrySnapshot {
  co2: number;
  temp: number;
  humid: number;
  lastReadOk?: boolean;
  history?: TelemetryHistorySnapshot;
}

export interface SensorTelemetryEvent {
  co2: number;
  temp: number;
  humid: number;
  lastReadOk: boolean;
  timestamp_ms: number;
}

export type SnapshotChannel =
  | "shelly"
  | "alarms"
  | "ble"
  | "sensing"
  | "telemetry"
  | "system_status"
  | "airmouse"
  | "telegram"
  | "notif_stats";

export interface BleSensorConfig {
  mac: string;
  alias: string;
}

export interface BleSettings {
  enabled: boolean;
  sensors?: BleSensorConfig[];
}

export interface BleDevice {
  mac: string;
  temp: number;
  humid: number;
  batt: number;
  rssi: number;
  last_seen: number;
  saved?: boolean;
}

export interface BleStatus {
  enabled: boolean;
  running: boolean;
  scanner_active?: boolean;
  metrics?: {
    adv_total: number;
    valid_packets: number;
    parser_errors: number;
    cache_drops: number;
    mutex_timeouts: number;
    scanner_running: boolean;
  };
  settings?: BleSettings;
  devices?: BleDevice[];
}

export interface BleDeviceEvent {
  mac: string;
  temp: number;
  humid: number;
  batt: number;
  rssi: number;
  lastSeen: number;
}

export interface ShellyDevice {
  id: string;
  name: string;
  isOn: boolean;
  isOnline: boolean;
  ip?: string;
  relayIndex?: number;
  enabled?: boolean;
  generation?: number;
  lastUpdate?: number;
  power?: number;
  energy?: number;
  voltage?: number;
  current?: number;
  temp?: number;
  rssi?: number;
}

export interface ShellyDeviceEvent {
  id: string;
  isOn: boolean;
  isOnline: boolean;
  power?: number;
  energy?: number;
  voltage?: number;
  current?: number;
  temp?: number;
  rssi?: number;
}

export interface SignInRequest {
  username: string;
  password: string;
}

export interface SignInResponse {
  access_token: string;
}

export interface AccessTokenPayload {
  username?: string;
  admin?: boolean;
  iat?: number;
}

export interface DeviceRecord {
  id: string;
  name: string;
  origin: string;
  input: string;
  createdAt: string;
  lastConnectedAt?: string;
}

export interface DeviceSession {
  accessToken: string;
  username: string;
  admin: boolean;
  signedInAt: string;
}

export interface LoggingConfig {
  level: string;
}

export interface AppConfig {
  logging?: LoggingConfig;
  [key: string]: unknown;
}

export interface WifiRecoveryResponse {
  success: boolean;
  accepted: boolean;
  connected: boolean;
  ip?: string;
  rssi?: number;
}

export interface TaskInfo {
  name: string;
  priority: number;
  stackHighWaterMark: number;
  state: string;
  coreId?: number;
}

export interface TasksResponse {
  watchdog: {
    initialized: boolean;
    timeoutSec: number;
  };
  taskCount: number;
  detailsIncluded?: boolean;
  tasks?: TaskInfo[];
  error?: string;
  memory: {
    freeHeap: number;
    minFreeHeap: number;
    freePsram?: number;
  };
}

export interface SystemNetworkInfo {
  wifi: {
    state: string;
    configured_mode: string;
    mode: string;
    mode_id: number;
    sta_connected: boolean;
    ap_active: boolean;
    last_disconnect_reason: number;
    last_ip_change_ms: number;
    disconnected_since_ms: number;
    stable_connected_since_ms: number;
    last_recovery_reason: string;
    sta_ip?: string;
    saved_static_ip?: string;
  };
  ap: {
    active: boolean;
    station_num: number;
    ip?: string;
  };
  http: {
    active_clients: number;
    peak_clients: number;
    opens: number;
    closes: number;
    last_open_ms: number;
    last_close_ms: number;
    ws_active_clients: number;
    ws_peak_clients: number;
    ws_opens: number;
    ws_closes: number;
    last_ws_open_ms: number;
    last_ws_close_ms: number;
    ws_forced_removals: number;
    ws_queue_drops: number;
    last_ws_queue_drop_ms: number;
    last_ws_queue_drop_payload: number;
    ws_heap_fallbacks: number;
    last_ws_heap_fallback_ms: number;
    last_ws_heap_fallback_payload: number;
    max_ws_heap_fallback_payload: number;
  };
  forwarding: {
    ready: boolean;
    requires_static_ip: boolean;
    saved_static_ip_configured: boolean;
    saved_static_ip_matches: boolean;
    https_port: number;
  };
}

export interface WifiStatus {
  status: number;
  local_ip?: string;
  mac_address?: string;
  rssi?: number;
  ssid?: string;
  bssid?: string;
  channel?: number;
  subnet_mask?: string;
  gateway_ip?: string;
  dns_ip_1?: string;
  dns_ip_2?: string;
}

export type WifiMode = "off" | "ap" | "sta";

export interface KnownNetworkItem {
  ssid: string;
  password: string;
  static_ip_config: boolean;
  local_ip?: string;
  subnet_mask?: string;
  gateway_ip?: string;
  dns_ip_1?: string;
  dns_ip_2?: string;
}

export interface WifiSettings {
  hostname: string;
  mode: WifiMode;
  /** Legacy websocket snapshot field; REST settings use `mode`. */
  connection_mode?: number;
  wifi_networks: KnownNetworkItem[];
}

export type NetworkScanState = "idle" | "running" | "ready";

export interface NetworkItem {
  rssi: number;
  ssid: string;
  bssid: string;
  channel: number;
  encryption_type: number;
}

export interface NetworkListResponse {
  networks: NetworkItem[];
  scan_state?: NetworkScanState;
}

export interface NtpSettings {
  enabled: boolean;
  server: string;
  tz_label: string;
  tz_format: string;
}

export interface NtpStatus {
  status: number;
  time_valid?: boolean;
  utc_time: string;
  local_time: string;
  server: string;
  uptime: number;
}

export interface ApStatus {
  status: number;
  ip_address: string;
  mac_address: string;
  station_num: number;
}

export interface ApSettings {
  ssid: string;
  password: string;
  channel: number;
  ssid_hidden: boolean;
  max_clients: number;
  local_ip: string;
  gateway_ip: string;
  subnet_mask: string;
}

export interface DiagnosticsHeapRegion {
  name: string;
  available: boolean;
  caps: number;
  total: number;
  free: number;
  minimumFree: number;
  largestBlock: number;
  fragmentationPercent: number;
}

export interface DiagnosticsHeapResponse {
  schema: "diagnostics.heap.v1";
  regions: {
    default: DiagnosticsHeapRegion;
    internal: DiagnosticsHeapRegion;
    psram: DiagnosticsHeapRegion;
  };
}

export interface DiagnosticsHttpHealth {
  activeClients: number;
  peakClients: number;
  opens: number;
  closes: number;
  lastOpenMs: number;
  lastCloseMs: number;
  wsActiveClients: number;
  wsPeakClients: number;
  wsOpens: number;
  wsCloses: number;
  lastWsOpenMs: number;
  lastWsCloseMs: number;
  wsForcedRemovals: number;
  wsQueueDrops: number;
  lastWsQueueDropMs: number;
  lastWsQueueDropPayload: number;
  wsHeapFallbacks: number;
  lastWsHeapFallbackMs: number;
  lastWsHeapFallbackPayload: number;
  maxWsHeapFallbackPayload: number;
}

export interface DiagnosticsSummaryResponse {
  schema: string;
  firmware: {
    name: string;
    version: string;
    buildTarget: string;
  };
  uptimeSec: number;
  boot: {
    bootCount: number;
    unexpectedRestarts: number;
    lastBootUnexpected: boolean;
    lastSessionUptimeMs: number;
    lastShutdownReason: number;
    lastResetReason: number;
    currentResetReason: number;
    freeHeapAtShutdown: number;
  };
  watchdog: {
    initialized: boolean;
    timeoutSec: number;
  };
  heap: {
    internalFree: number;
    internalMinimumFree: number;
    internalLargestBlock: number;
    internalFragmentationPercent: number;
    psramFree: number;
    psramMinimumFree: number;
    psramLargestBlock: number;
    psramFragmentationPercent: number;
  };
  http: DiagnosticsHttpHealth;
  features: {
    configRead: boolean;
    count: number;
  };
}

export interface DiagnosticsLockCounter {
  attempts: number;
  successes: number;
  timeouts: number;
  slowAcquires: number;
  unlimitedWaits: number;
  maxWaitTicks: number;
  maxWaitMs: number;
}

export interface DiagnosticsMutexesResponse {
  schema: "diagnostics.mutexes.v1";
  instrumented: boolean;
  coverage: {
    contentionCounters: boolean;
    holdTimeBuckets: boolean;
    timeoutCounters: boolean;
    slowAcquireCounters: boolean;
    perLockNames: boolean;
  };
  runtime: {
    slowThresholdTicks: number;
    slowThresholdMs: number;
    standard: DiagnosticsLockCounter;
    recursive: DiagnosticsLockCounter;
  };
  reason: string;
  criticalLocks: Array<{
    name: string;
    instrumented: boolean;
    counterScope: string;
  }>;
}

export interface DiagnosticsEndpointEntry {
  path: string;
  method: string;
  auth: string;
  description: string;
}

export interface DiagnosticsEndpointsResponse {
  schema: "diagnostics.endpoints.v1";
  metrics: {
    requestCounts: boolean;
    errorCounts: boolean;
    latencyBuckets: boolean;
    httpTransportCounters: boolean;
  };
  diagnostics: DiagnosticsEndpointEntry[];
}

export interface DiagnosticsFeatureState {
  key: string;
  serviceAvailable: boolean;
  configKnown: boolean;
  configuredEnabled: boolean;
  runtimeMeasured: boolean;
  runtimeActive: boolean;
  detail?: string;
}

export interface DiagnosticsFeaturesResponse {
  schema: "diagnostics.features.v1";
  configRead: boolean;
  features: DiagnosticsFeatureState[];
}

export interface PowerConfig {
  sleep_enabled: boolean;
  inactivity_timeout_ms: number;
  grace_after_boot_ms: number;
  wake_timer_enabled: boolean;
  wake_button_enabled: boolean;
  wake_touch_enabled: boolean;
  wake_interval_ms: number;
  timer_wake_awake_ms: number;
  button_wake_awake_ms: number;
  wake_touch_gpio: number;
  wake_touch_threshold: number;
}

export interface PowerStatus extends PowerConfig {
  wake_reason: string;
  wake_cause_raw: number;
  wake_gpio_mask: string;
  wake_ext1_mask: string;
  sleep_requested: boolean;
  sleep_eta_ms: number;
  wake_awake_window_ms: number;
  wake_awake_eta_ms: number;
  last_activity_ms: number;
  thermal_state:
    | "normal"
    | "soft_throttle"
    | "hard_throttle"
    | "critical"
    | "unknown";
  thermal_temp_c: number | null;
  thermal_cpu_mhz: number;
  thermal_throttled: boolean;
  thermal_soft_c: number;
  thermal_hard_c: number;
  thermal_critical_c: number;
  uptime_ms: number;
}

export interface RssiSample {
  rssi: number;
  timestamp: number;
  variance?: number;
}

export interface WifiSensingStats {
  current: number;
  filtered?: number;
  min: number;
  max: number;
  avg: number;
  variance: number;
  sampleCount: number;
  windowMs: number;
}

export interface WifiSensingData {
  enabled: boolean;
  running: boolean;
  active: boolean;
  connectedSSID: string;
  connectedChannel: number;
  stats: WifiSensingStats;
  motionDetected: boolean;
  variance_threshold: number;
  samples?: RssiSample[];
}

export interface WifiSensingSettings {
  enabled: boolean;
  sample_interval_ms: number;
  variance_threshold: number;
  csi_alarm: CsiAlarmSettings;
}

export interface CsiAlarmBand {
  start: number;
  end: number;
}

export interface CsiAlarmSettings {
  enabled: boolean;
  bands: CsiAlarmBand[];
  baseline_frames: number;
  top_k: number;
  enter_threshold: number;
  clear_threshold: number;
  hold_ms: number;
  clear_hold_ms: number;
  min_noise: number;
  min_energy: number;
  noisy_threshold: number;
  auto_recalibration: boolean;
  sensitivity: 0 | 1 | 2;
}

export interface CsiMotionStatus {
  enabled: boolean;
  state: string;
  baseline_ready: boolean;
  detected: boolean;
  decision_valid: boolean;
  has_frame: boolean;
  data_fresh: boolean;
  last_frame_ms: number;
  frame_age_ms: number;
  noisy: boolean;
  needs_calibration: boolean;
  score: number;
  confidence: number;
  frames_seen: number;
  width: number;
  band_count: number;
  selected_carriers: number;
  valid_carriers: number;
  last_reset_reason: string;
}

export interface CsiRuntimeMetrics {
  enabled: boolean;
  runtime_fault: boolean;
  runtime_reconcile_pending: boolean;
  queue_allocated: boolean;
  queue_metrics_valid: boolean;
  active_consumer_mask: number;
  consumer_count: number;
  frontend_consumer_active: boolean;
  alarm_consumer_active: boolean;
  boot_consumer_active: boolean;
  matrix_visualization_consumer_active: boolean;
  diagnostic_capture_consumer_active: boolean;
  queue_depth: number;
  queue_capacity: number;
  queue_drops_total: number;
  queue_drops_last_sec: number;
  rx_frames_total: number;
  rx_accepted_total: number;
  rx_throttled_total: number;
  queued_packets_total: number;
  dequeued_packets_total: number;
  packets_forwarded_total: number;
  batches_forwarded_total: number;
  batches_dropped_total: number;
  packets_per_sec: number;
  batches_per_sec: number;
  last_packet_ms: number;
  last_batch_ms: number;
  motion_control_epoch: number;
  calibration_count: number;
  calibration_target: number;
  calibration_state: string;
  motion: CsiMotionStatus;
  ws_client_count: number;
  ws_queue_enabled: boolean;
  capture: CsiCaptureRuntimeMetrics;
}

export interface CsiCaptureRuntimeMetrics {
  client_count: number;
  queue_enabled: boolean;
  starting: boolean;
  accepting: boolean;
  stopping: boolean;
  session_id: number;
  start_exclusive_sequence: number;
  stop_inclusive_sequence: number;
  records_offered: number;
  records_enqueued: number;
  records_dropped: number;
  truncated_records: number;
}

export interface CsiCalibrationResponse {
  ok: true;
  state: "calibrating";
}

export interface WifiSensingStatus extends WifiSensingData {
  schema: "wifisensing.status.v1";
  sample_interval_ms: number;
  csi: CsiRuntimeMetrics;
}

export type AlarmSource =
  | "co2"
  | "temperature"
  | "humidity"
  | "wifi_motion"
  | "ble_temperature"
  | "ble_humidity"
  | "ble_battery"
  | "ble_rssi"
  | "wifi_csi_motion"
  | "imu_tamper"
  | "gpio_digital";

export type AlarmOperator = "above" | "below";
export type NotifyChannel = "telegram" | "led" | "webhook" | "pushover";
export type AlarmSeverity = "info" | "warning" | "critical";

export interface AlarmRule {
  id: string;
  enabled: boolean;
  name: string;
  source: AlarmSource;
  operator: AlarmOperator;
  threshold: number;
  severity: AlarmSeverity;
  notify_channels: NotifyChannel[];
  cooldown_seconds: number;
  shelly_device_ids?: string[];
  ble_device_mac?: string;
  created_at?: number;
  updated_at?: number;
  triggered?: boolean;
  last_triggered?: number;
  current_value?: number;
}

export interface AlarmRulesConfig {
  schema_version: 1;
  rules: AlarmRule[];
}

export interface LogFile {
  name: string;
  size: number;
}

export interface LogMonth {
  name: string;
  path: string;
  files: LogFile[];
}

export interface LogListResponse {
  total_size?: number;
  months: LogMonth[];
}

export interface TailLine {
  t: number;
  l: string;
  g: string;
  m: string;
}

export interface TailResponse {
  capacity?: number;
  lines: TailLine[];
}

export interface TailClearResponse {
  ok: boolean;
  status: string;
}

export interface NotificationSettings {
  telegram_enabled: boolean;
  webhook_enabled: boolean;
  bot_token: string;
  chat_id: string;
  commands_enabled: boolean;
  webhook_url: string;
  pushover_enabled: boolean;
  pushover_user: string;
  pushover_token: string;
  is_configured: boolean;
}

export interface NotificationTestResult {
  ok: boolean;
  configured?: boolean;
  httpCode?: number;
  error?: string;
  response?: string;
}

export interface ScriptStatus {
  current_script: string;
  status: "IDLE" | "RUNNING" | "PAUSED" | "ERROR" | "COMPLETED";
  current_line: number;
  uptime_ms: number;
  last_error: string;
}

export interface ScriptFile {
  name: string;
}

export interface MacroSettings {
  enabled: boolean;
  boot_script: string;
  boot_delay: number;
}

export interface MacroActionResponse {
  ok: boolean;
  status?: "saved" | "deleted" | "started" | "stopped" | "no_changes";
  error?: string;
}

export interface HeartbeatSlot {
  enabled: boolean;
  name: string;
  url: string;
  allow_insecure: boolean;
}

export interface HeartbeatSettings {
  interval_ms: number;
  slots: HeartbeatSlot[];
}

export type HeartbeatTestStatus = "queued" | "no_enabled_slots" | "ping_failed";

export interface HeartbeatTestResult {
  success: boolean;
  message: string;
  status?: HeartbeatTestStatus;
  active_slots?: number;
  retry_count?: number;
  timeout_ms?: number;
  retry_after_ms?: number;
}

export type UdpFormat = "line" | "json" | "csv";

export interface UdpSettings {
  enabled: boolean;
  host: string;
  port: number;
  format: UdpFormat;
  interval_ms: number;
}

export type UdpTestStatus =
  | "queued"
  | "sent"
  | "not_configured"
  | "worker_stopping"
  | "wifi_disconnected"
  | "send_failed"
  | "unavailable";

export interface UdpTestResult {
  success: boolean;
  message: string;
  status?: UdpTestStatus;
  retry_after_ms?: number;
}

export interface CompensationSettings {
  enabled: boolean;
  base_temp_offset: number;
  reference_cpu_temp: number;
  temp_offset_per_cpu_degree: number;
  min_temp_offset: number;
  max_temp_offset: number;
}
