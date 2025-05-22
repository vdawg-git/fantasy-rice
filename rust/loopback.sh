#!/bin/bash
# Setup script for Linux audio monitoring

# Detect audio system
if command -v pactl &> /dev/null; then
    AUDIO_SYSTEM="pulseaudio"
    echo "Detected PulseAudio audio system"
elif command -v pw-cli &> /dev/null; then
    AUDIO_SYSTEM="pipewire"
    echo "Detected PipeWire audio system"
else
    echo "Could not detect PulseAudio or PipeWire. Please install one of these audio systems."
    exit 1
fi

# Setup audio loopback based on detected system
if [ "$AUDIO_SYSTEM" = "pulseaudio" ]; then
    echo "Setting up PulseAudio loopback..."
    
    # Check if module-loopback is already loaded
    if pactl list modules | grep -q "module-loopback"; then
        echo "PulseAudio loopback module is already loaded."
    else
        # Load the loopback module with low latency
        pactl load-module module-loopback latency_msec=1
        echo "PulseAudio loopback module loaded successfully."
    fi
    
    echo "Tip: Use 'pavucontrol' (PulseAudio Volume Control) to configure loopback settings."
    
elif [ "$AUDIO_SYSTEM" = "pipewire" ]; then
    echo "Setting up PipeWire loopback..."
    
    # Check if pw-loopback is already running
    if pgrep -f "pw-loopback" &> /dev/null; then
        echo "PipeWire loopback is already running."
    else
        # Start pw-loopback in the background
        pw-loopback -m '[FL FR]' --capture-props='media.class=Audio/Sink' &>/dev/null &
        echo "PipeWire loopback started successfully."
    fi
    
    echo "Tip: Use 'pw-top' to monitor your PipeWire setup."
fi

echo "Audio loopback setup complete!"
echo "You can now run your audio monitoring application."
echo ""
echo "To start the Unix socket listener:"
echo "  ./socket_listener.sh"
echo ""
echo "To run the audio loudness monitor:"
echo "  cargo run --release"