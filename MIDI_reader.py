import time
import board
import busio
import usb_midi
import adafruit_midi
from adafruit_midi.note_on import NoteOn
from adafruit_midi.note_off import NoteOff
from adafruit_pca9685 import PCA9685

START_NOTE = 21   # A0
END_NOTE = 108    # C8
NUM_KEYS = END_NOTE - START_NOTE + 1  # 88 keys
PWM_DUTY_ON = 0xFFFF      # Full-on (65535)
PWM_DUTY_OFF = 0x0000     # Full-off
PWM_MIN_ACTIVATION = 56000  # ~85% of max

i2c = busio.I2C(scl=board.GP3, sda=board.GP2)

pca_list = []
for addr in range(0x40, 0x46):
    pca = PCA9685(i2c, address=addr)
    pca.frequency = 1000  # Hz, 100 was buzzing
    pca_list.append(pca)

midi = adafruit_midi.MIDI(midi_in=usb_midi.ports[0], in_channel=None) # None listens on all channels, set channel to listen on a specific one


def get_pca_channel(note):
    if START_NOTE <= note <= END_NOTE:l
        index = note - START_NOTE
        pca_index = index // 16
        channel_index = index % 16
        return pca_list[pca_index], channel_index
    return None, None

def velocity_to_pwm(velocity):
    velocity = max(0, min(127, velocity))
    return int(PWM_MIN_ACTIVATION + (PWM_DUTY_ON - PWM_MIN_ACTIVATION) * (velocity / 127))

while True:
    msg = midi.receive()

    if isinstance(msg, NoteOn) and msg.velocity > 0:
        note = msg.note
        pwm_value = velocity_to_pwm(msg.velocity)
        pca, channel = get_pca_channel(note)
        if pca is not None:
            print(f"Note ON: {note}, Velocity: {msg.velocity}, PWM: {pwm_value}")
            pca.channels[channel].duty_cycle = pwm_value
            
    elif isinstance(msg, NoteOff) or (isinstance(msg, NoteOn) and msg.velocity == 0):
        note = msg.note
        pca, channel = get_pca_channel(note)
        if pca is not None:
            print(f"Note OFF: {note}")
            pca.channels[channel].duty_cycle = PWM_DUTY_OFF
