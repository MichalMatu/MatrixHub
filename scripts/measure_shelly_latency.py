import requests
import time
import sys
import argparse

def measure_shelly(ip, count=10):
    url = f"http://{ip}/rpc/Shelly.GetStatus"
    print(f"Measuring latency to Shelly at {url} ({count} requests)...")
    
    timings = []
    errors = 0
    
    for i in range(count):
        start = time.time()
        try:
            resp = requests.get(url, timeout=2.0)
            duration = (time.time() - start) * 1000
            
            if resp.status_code == 200:
                print(f"Req #{i+1}: {duration:.2f} ms")
                timings.append(duration)
            else:
                print(f"Req #{i+1}: HTTP {resp.status_code} ({duration:.2f} ms)")
                errors += 1
                
        except requests.exceptions.Timeout:
            print(f"Req #{i+1}: TIMEOUT (>2000 ms)")
            errors += 1
        except Exception as e:
            print(f"Req #{i+1}: ERROR {e}")
            errors += 1
            
        time.sleep(0.5)
        
    if timings:
        avg = sum(timings) / len(timings)
        print(f"\nStats:")
        print(f"  Min: {min(timings):.2f} ms")
        print(f"  Max: {max(timings):.2f} ms")
        print(f"  Avg: {avg:.2f} ms")
        print(f"  Success Rate: {((count-errors)/count)*100:.1f}%")
    else:
        print("\nNo successful requests.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 measure_shelly_latency.py <SHELLY_IP>")
        sys.exit(1)
        
    measure_shelly(sys.argv[1])
