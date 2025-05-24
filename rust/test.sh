#!/bin/bash
# Simple Unix socket listener script to verify the audio loudness data

SOCKET_PATH="/tmp/audio_monitor.sock"

# Check if socat is installed
if ! command -v socat &> /dev/null; then
    echo "Error: socat is not installed. Please install it to run this script."
    exit 1
fi


echo "Connecting to socket at $SOCKET_PATH"
echo "Press Ctrl+C to stop"

# Listen on the Unix socket and display received data
socat - UNIX-CONNECT:"$SOCKET_PATH"
