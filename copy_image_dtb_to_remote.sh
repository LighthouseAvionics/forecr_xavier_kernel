#!/bin/bash

# Usage: ./script_name.sh [local_path] [remote_username] [remote_hostname]

LOCAL_PATH=$1
REMOTE_USER=$2
REMOTE_HOST=$3
REMOTE_SUBDIR=$4

# Check if the mandatory arguments are provided
if [ -z "$LOCAL_PATH" ] || [ -z "$REMOTE_HOST" ] || [ -z "$REMOTE_USER" ]; then
    echo "Usage: $0 [local_path] [remote_hostname] [remote_username] [optional: remote_subdir]"
    exit 1
fi

# Define the files to copy
FILE1="$LOCAL_PATH/arch/arm64/boot/Image"
FILE2="$LOCAL_PATH/arch/arm64/boot/dts/nvidia/tegra234-p3701-0005-p3737-0000-dsboard-agx.dtb"

# Check if the files exist
if [ ! -f "$FILE1" ] || [ ! -f "$FILE2" ]; then
    echo "One or both files do not exist in the specified directory."
    exit 1
fi

# Determine the remote directory
REMOTE_DIR="$REMOTE_USER@$REMOTE_HOST:~/Documents"
if [ ! -z "$REMOTE_SUBDIR" ]; then
    REMOTE_DIR="$REMOTE_DIR/$REMOTE_SUBDIR"
fi

# Copying files
scp "$FILE1" "$REMOTE_DIR/"
scp "$FILE2" "$REMOTE_DIR/"

echo "File transfer complete."
