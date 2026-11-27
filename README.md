# RockstarConnected
This is an Xbox 360 plugin for Grand Theft Auto V Title Update 13 (1.12) which patches natives to tell the game that its connected to Rockstar Services and using a glitch to access all of the Online content* in temp character.

*Currently unsure if it grants you the FULL online stuff, ive tested the same on PS3 and you unlock everything. Ive only been able to test it out on xenia but its very unstable and crashes a lot. Will soon test on actual Xbox 360 hardware to see how far i get

You can test this out using the XEX [here](https://github.com/Parkeriscoo/RockstarConnected/raw/refs/heads/main/RockstarConnected/Release/RockstarConnected.xex)

## How it works
The XEX patches a handful of natives that tells the game its "connected" to rockstar services

## How to activate the glitch
Enable the XEX before launching GTA V, once you launch the game try to connect to online (do not connect to online from the menu. connect via the main menu.)

Once you launch online you will be sent to the sky and stuck joining session. Back out and relaunch online. You will be asked about the typical temp charater prompt but after the third message you will be kicked out back to story mode. rejoin online, and after the three temp popups you should be able to create your temp character and join temp online.

Once you are connected to temp online pause the game and launch the creator, once its launched exit to GTA Online. You should be asked if you want to change your appearance. You can either change or skip it.

After that you will be loaded into the tutorial! (with the race part skipped. I tried to skip the whole tutorial but it wouldnt make the glitch work.)

After you complete the GTA Online tutorial you are now in the "full" online state.

## Xenia plugins.toml
title_name = "Grand Theft Auto V"
title_id = "545408A7"

[[plugin]]
name = "RockstarConnected"
file = "RockstarConnected.xex"
hash = "829BD9C4A5B1F3C0" # TU 13
desc = "native shit"
is_enabled = true
