import argparse
import csv
import statistics
from pathlib import Path

DEFAULT_INPUT_FILE = Path(__file__).with_name("raw_capture.csv")
DEFAULT_THRESHOLD = 5


def parse_args():
    parser = argparse.ArgumentParser(
        description="Analyze raw WiFi sensing CSV captures for repeated RSSI drop patterns."
    )
    parser.add_argument(
        "input_file",
        nargs="?",
        default=str(DEFAULT_INPUT_FILE),
        help="Path to the CSV capture file.",
    )
    parser.add_argument(
        "--threshold",
        type=int,
        default=DEFAULT_THRESHOLD,
        help="RSSI drop in dBm required to start an event.",
    )
    return parser.parse_args()


def load_samples(input_file: Path):
    if not input_file.is_file():
        raise FileNotFoundError(f"Input file not found: {input_file}")

    samples = []
    with input_file.open("r", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            samples.append({
                "ts": int(row["timestamp"]),
                "rssi": int(row["rssi"]),
            })
    return samples


def analyze(input_file: Path, threshold: int):
    print(f"Deep Analysis of {input_file}...")

    samples = load_samples(input_file)
    if not samples:
        print("No data.")
        return

    # 1. Baseline Calculation
    # Simple approx moving average for baseline
    baseline = samples[0]['rssi']
    
    events = []
    in_event = False
    event_start_ts = 0
    event_min_rssi = 0
    event_start_index = 0
    
    # Analyze Events
    for i, s in enumerate(samples):
        rssi = s['rssi']
        
        # Update baseline slowly only if value is "normal" (not in deep drop)
        if abs(rssi - baseline) < 5: 
             baseline = baseline * 0.95 + rssi * 0.05
        
        diff = rssi - baseline
        
        if diff < -threshold:
            if not in_event:
                in_event = True
                event_start_ts = s['ts']
                event_min_rssi = rssi
                event_start_index = i
            else:
                if rssi < event_min_rssi:
                    event_min_rssi = rssi
        else:
            if in_event:
                # Event ended
                duration = s['ts'] - event_start_ts
                drop = int(baseline - event_min_rssi)
                
                # Check "instantness" (Slope)
                # Look at sample immediately before event vs event start
                prev_rssi = samples[event_start_index-1]['rssi'] if event_start_index > 0 else baseline
                first_drop_rssi = samples[event_start_index]['rssi']
                instant_drop_val = prev_rssi - first_drop_rssi
                
                events.append({
                    'start_ts': event_start_ts,
                    'min_rssi': event_min_rssi,
                    'baseline': int(baseline),
                    'drop_dbm': drop,
                    'duration_ms': duration,
                    'instant_drop': instant_drop_val
                })
                in_event = False
    
    print(f"\nFound {len(events)} events. Searching for COMMON PATTERNS:\n")
    
    # Collection values
    min_rssis = [e['min_rssi'] for e in events]
    drops = [e['drop_dbm'] for e in events]
    instant_drops = [e['instant_drop'] for e in events]
    durations = [e['duration_ms'] for e in events]
    
    print(f"1. TARGET RSSI (Bottom of Spike):")
    print(f"   Values: {sorted(min_rssis)}")
    modes = statistics.multimode(min_rssis) if min_rssis else []
    mode_label = ", ".join(str(mode) for mode in modes) if modes else "None"
    print(f"   Mode (Most Common): {mode_label}")
    
    print(f"\n2. DROP MAGNITUDE:")
    print(f"   Values: {sorted(drops)}")
    
    print(f"\n3. INSTANT DROP (Frame 0 magnitude):")
    print(f"   Values: {sorted(instant_drops)}")
    print(f"   Note: High value means Instant/Square edge.")

    print(f"\n4. DURATION:")
    print(f"   Values: {sorted(durations)}")

    print(f"\n5. DETAILED EVENT LIST:")
    print(f"{'#':<3} {'Start':<10} {'Base':<5} {'Min':<5} {'Drop':<6} {'InstDrop':<8} {'Dur(ms)':<8}")
    for i, e in enumerate(events):
         print(f"{i+1:<3} {e['start_ts']:<10} {e['baseline']:<5} {e['min_rssi']:<5} -{e['drop_dbm']:<5} -{e['instant_drop']:<7} {e['duration_ms']:<8}")

    # --- CLUSTERING LOGIC ---
    print(f"\n6. APPROACH CLUSTERING (Merging events < 3s apart):")
    if not events:
        print("No events to cluster.")
        return

    clusters = []
    current_cluster = {
        'start_ts': events[0]['start_ts'],
        'end_ts': events[0]['start_ts'] + events[0]['duration_ms'],
        'event_count': 1,
        'max_drop': events[0]['drop_dbm']
    }
    
    for i in range(1, len(events)):
        e = events[i]
        # Check gap between end of last cluster and start of this event
        gap = e['start_ts'] - current_cluster['end_ts']
        
        if gap < 3000: # 3 seconds merge window
            # Merge
            current_cluster['end_ts'] = e['start_ts'] + e['duration_ms']
            current_cluster['event_count'] += 1
            current_cluster['max_drop'] = max(current_cluster['max_drop'], e['drop_dbm'])
        else:
            # Save and Start New
            clusters.append(current_cluster)
            current_cluster = {
                'start_ts': e['start_ts'],
                'end_ts': e['start_ts'] + e['duration_ms'],
                'event_count': 1,
                'max_drop': e['drop_dbm']
            }
    clusters.append(current_cluster) # Save last one

    print(f"Found {len(clusters)} Distinct Approaches (Clusters):")
    for i, c in enumerate(clusters):
        duration_s = (c['end_ts'] - c['start_ts']) / 1000.0
        print(f"  Cluster #{i+1}: Duration {duration_s:.1f}s, Events: {c['event_count']}, Max Drop: -{c['max_drop']}dBm")

if __name__ == "__main__":
    arguments = parse_args()
    try:
        analyze(Path(arguments.input_file), arguments.threshold)
    except Exception as exc:
        raise SystemExit(f"Error: {exc}")
