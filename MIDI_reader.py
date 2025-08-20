import time
import board
import busio
import usb_midi
import adafruit_midi
from adafruit_midi.note_on import NoteOn
from adafruit_midi.note_off import NoteOff
from adafruit_pca9685 import PCA9685

# Constants
START_NOTE = 21   # A0
END_NOTE = 108    # C8
NUM_KEYS = END_NOTE - START_NOTE + 1  # 88 keys
PWM_DUTY_ON = 0xFFFF      # Full-on (65535)
PWM_DUTY_OFF = 0x0000     # Full-off
PWM_MIN_ACTIVATION = 60000  # Minimum PWM value to activate solenoid (~85% of max)

# I2C setup
i2c = busio.I2C(scl=board.GP3, sda=board.GP2)

# Try to create up to 6 PCA9685 boards (16 channels each)
pca_list = []
for addr in range(0x40, 0x46):  # Addresses 0x40–0x45
    try:
        pca = PCA9685(i2c, address=addr)
        pca.frequency = 1000  # Hz, fine for solenoids
        pca_list.append(pca)
        print(f"✅ Found PCA9685 at address 0x{addr:02X}")
    except ValueError:
        print(f"⚠️ No PCA9685 at address 0x{addr:02X}, skipping...")

NUM_AVAILABLE_CHANNELS = len(pca_list) * 16
print(f"Total available channels: {NUM_AVAILABLE_CHANNELS}")

# MIDI setup: listens on all channels
midi = adafruit_midi.MIDI(midi_in=usb_midi.ports[0], in_channel=None)

# Mapping MIDI note to PCA9685 channel
def get_pca_channel(note):
    if START_NOTE <= note <= END_NOTE:
        index = note - START_NOTE
        if index < NUM_AVAILABLE_CHANNELS:  # only map if enough channels
            pca_index = index // 16
            channel_index = index % 16
            return pca_list[pca_index], channel_index
    return None, None

# Convert MIDI velocity (0-127) to PWM value (60000–65535)
def velocity_to_pwm(velocity):
    velocity = max(0, min(127, velocity))  # Clamp
    return int(PWM_MIN_ACTIVATION + (PWM_DUTY_ON - PWM_MIN_ACTIVATION) * (velocity / 127))

# Main loop
while True:
    msg = midi.receive()

    if isinstance(msg, NoteOn) and msg.velocity > 0:
        # Note On with velocity
        note = msg.note
        pwm_value = velocity_to_pwm(msg.velocity)
        pca, channel = get_pca_channel(note)
        if pca is not None:
            print(f"Note ON: {note}, Velocity: {msg.velocity}, PWM: {pwm_value}")
            pca.channels[channel].duty_cycle = pwm_value
        else:
            print(f"Note ON ignored: {note} (no channel available)")

    elif isinstance(msg, NoteOff) or (isinstance(msg, NoteOn) and msg.velocity == 0):
        # Note Off
        note = msg.note
        pca, channel = get_pca_channel(note)
        if pca is not None:
            print(f"Note OFF: {note}")
            pca.channels[channel].duty_cycle = PWM_DUTY_OFF
        else:
            print(f"Note OFF ignored: {note} (no channel available)")
