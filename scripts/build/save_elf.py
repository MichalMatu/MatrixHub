import shutil
import re
import os
Import("env")
import hashlib


OUTPUT_DIR = "build{}elf{}".format(os.path.sep,os.path.sep)

def elf_copy(source, target, env):
    # check if output directories exist and create if necessary
    if not os.path.isdir("build"):
        os.mkdir("build")

    if not os.path.isdir(OUTPUT_DIR):
        os.mkdir(OUTPUT_DIR)

    with open(str(target[0]),"rb") as f:
        result = hashlib.sha256(f.read())
        
        # create string with location and file names based on variant
        elf_file = "{}{}.elf".format(OUTPUT_DIR, result.hexdigest())

        # check if new target files exist and remove if necessary
        for f in [elf_file]:
            if os.path.isfile(f):
                os.remove(f)

        print("Saving ELF file to "+elf_file)

        # copy firmware.bin to firmware/<variant>.bin
        shutil.copy(str(target[0]), elf_file)

        # Maintain a stable pointer to the freshest ELF so diagnostic tooling
        # (e.g. scripts/diagnostics/decode_coredump.py) can run zero-config.
        latest = "{}latest.elf".format(OUTPUT_DIR)
        try:
            if os.path.islink(latest) or os.path.isfile(latest):
                os.remove(latest)
            os.symlink(os.path.basename(elf_file), latest)
        except OSError:
            shutil.copy(elf_file, latest)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", [elf_copy])