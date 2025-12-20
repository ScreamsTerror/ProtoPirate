# SConstruct file for ProtoPirate

import os

# Exclude temp_honda_firmware from build
env = Environment()

# Get list of all C files, excluding temp_honda_firmware
c_files = []
for root, dirs, files in os.walk('.'):
    # Skip temp_honda_firmware directory
    if 'temp_honda_firmware' in root:
        continue
    if '.git' in root:
        continue
    if 'dist' in root:
        continue
    
    for file in files:
        if file.endswith('.c'):
            c_files.append(os.path.join(root, file))

# Build the application
env.Program(target='protopirate', source=c_files)
