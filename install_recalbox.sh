#!/bin/bash

# script to install pocketcdg on recalbox
# exec it with the command
#
# curl https://raw.githubusercontent.com/redbug26/libretro-pocketcdg/master/install_recalbox.sh | sh
#
#

#CONFIG=es_systems.cfg
CONFIG=/recalbox/share_init/system/.emulationstation/es_systems.cfg

mount -o remount, rw /

# Copy the pocketcdg core in the libretro folder
curl http://www.kyuran.be/wp-content/uploads/2017/02/pocketcdg_libretro.so -o /usr/lib/libretro/pocketcdg_libretro.so

# Add .kcr file extension in emulation station
xml ed -u "/systemList/system[name='amstradcpc']/extension" -v ".dsk .DSK .zip .ZIP .kcr .KCR" $CONFIG >$CONFIG.new
cp $CONFIG.new $CONFIG

# Add pocketcdg core in emulation station if not exist yet
count=`xml sel -t -v "count(/systemList/system[name='amstradcpc']/emulators/emulator/cores[core='pocketcdg'])" $CONFIG`
if [ "$count" == "0" ]; then
        xml ed -s "/systemList/system[name='amstradcpc']/emulators/emulator/cores" -t elem -n core -v "pocketcdg" -i core -t attr -n class -v com.foo $CONFIG > $CONFIG.new
        cp $CONFIG.new $CONFIG
fi


# Verify the config
#xmlstarlet sel -t -c "/systemList/system[name='amstradcpc']" $CONFIG
