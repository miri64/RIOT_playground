# RIOT version

- use 2019.04 for now

# Config

- Channel: `DEFAULT_CHANNEL=19`
- Number of display LEDs: `LUKE_DISPLAY_CNT=64`

# Boot-up

1. start display
2. start button
3. button waits for display to be found (i.e. takes a few seconds to work
   properly)
