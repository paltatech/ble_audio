add_subdirectory(led_handler)
add_subdirectory(button_handler)

# audio_handler needs i2s0, which nrf5340dk leaves disabled (no codec chip wired
# up on this board, unlike the nRF5340 Audio DK) - see the include comment in
# app_streamctrl.c.
#
# add_subdirectory(audio_handler)

add_subdirectory(codec_handler)
add_subdirectory(ble_audio_handler)
