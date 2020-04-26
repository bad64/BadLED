# Changelog (26/4/20)

* Firmware
    * Changed data order in `get hwinfo` response (state flags, layout flags, delay, number of buttons, *then* color info)
    * Fixed an issue where I forgot to update where to store the delay flag in `setDelay()`
* Software
    * Adapted code to reflect the changes above
    * Switching COM ports appears to work now (needs more thorough testing)
    * Console now outputs in and outbound messages if the `USE_LOOPBACK` flag is set to 1
* Meta
    * Changelogs are now their own separate file
