import os
import shutil
from SCons.Script import Import

Import("env")

# List of Arduino libraries to disable in the global framework
# These are folder names in libraries/
LIBS_TO_DISABLE = [
    "Matter",
    "ESP-Matter",
    "Zigbee",
    "OpenThread",
    "RainMaker",
    "Insights",
    "WiFiProvisioning",
    "SD",
    "SD_MMC",
    "Ethernet",
    "PPP",
    "SimpleBLE",
    "USB",
    "USBHID",
    "SR",
    "ESP-SR"
]

def disable_arduino_libs(env):
    # Try to find the framework directory
    # In PlatformIO, it's typically in FRAMEWORK_DIR or via PIO package manager
    
    # We can try to deduce it from the environment or common paths
    # However, env['PIOFRAMEWORKDIR'] might not be set for all frameworks directly in this context
    # Let's inspect env['PLATFORM_MANIFEST'] or similar if possible, but simplest is to look at standard paths
    
    # A generic way to find the package directory:
    platform_packages = env.GetProjectOption("platform_packages", [])
    # This might not list the default ones.
    
    # Let's verify where the framework is. 
    # Usually ~/.platformio/packages/framework-arduinoespressif32
    
    home = os.path.expanduser("~")
    framework_dir = os.path.join(home, ".platformio", "packages", "framework-arduinoespressif32")
    
    if not os.path.exists(framework_dir):
        print(f"Warning: Framework directory not found at {framework_dir}")
        return

    libraries_dir = os.path.join(framework_dir, "libraries")
    if not os.path.exists(libraries_dir):
         print(f"Warning: Libraries directory not found at {libraries_dir}")
         return
         
    print(f"Checking for unused Arduino libraries in: {libraries_dir}")
    
    for lib in LIBS_TO_DISABLE:
        lib_path = os.path.join(libraries_dir, lib)
        cmake_file = os.path.join(lib_path, "CMakeLists.txt")
        disabled_file = os.path.join(lib_path, "CMakeLists.txt.disabled")
        
        if os.path.exists(cmake_file):
            print(f"Disabling library: {lib} (Renaming CMakeLists.txt -> .disabled)")
            try:
                os.rename(cmake_file, disabled_file)
            except Exception as e:
                print(f"Failed to disable {lib}: {e}")
        elif os.path.exists(lib_path) and os.path.exists(disabled_file):
            print(f"Library {lib} is already disabled.")
        elif os.path.exists(lib_path):
            print(f"Library {lib} exists but has no CMakeLists.txt (might be header only or already modified)")
        # else:
            # print(f"Library {lib} not found (OK)")

# Run the function
try:
    disable_arduino_libs(env)
except Exception as e:
    print(f"Error in disable_arduino_libs script: {e}")
