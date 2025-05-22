#!/bin/bash
# Simple Unix socket listener script to verify the audio loudness data

SOCKET_PATH="/tmp/audio_monitor.sock"

# Check if socat is installed
if ! command -v socat &> /dev/null; then
    echo "Error: socat is not installed. Please install it to run this script."
    exit 1
fi

# Remove the socket if it already exists
if [ -e "$SOCKET_PATH" ]; then
    rm "$SOCKET_PATH"
fi

echo "Starting socket listener on $SOCKET_PATH"
echo "Waiting for connections..."
echo "Press Ctrl+C to stop"

# Listen on the Unix socket and display received data
socat UNIX-LISTEN:"$SOCKET_PATH",fork -

# Alternative approach with netcat if available
# nc -lU "$SOCKET_PATH"