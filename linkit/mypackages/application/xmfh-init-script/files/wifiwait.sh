#! /bin/sh
# Not an OpenWrt init script. Do not install under /etc/init.d/
# (apk postinst would run this on install). Kept for later Wi-Fi work.

# Delays the broadcast of the WiFi Access Point until 
# either a flag file is detected or a timeout occurs.

echo "Broadcast of WiFi delayed." > /dev/console

start=`date +%s`

flagfile=/tmp/_wifiStart
timeout=60

while [ ! -f "$flagfile" ]
do
        if [[ $timeout -gt 0 ]]
        then
                timeout=$(($timeout-1))
                sleep 1
        else
                echo "Timeout exceeded for WiFi broadcast." > /dev/console
                break
        fi
done

# Operations to enable delayed WIFI AP broadcast
uci set wireless.radio0.disabled=0
uci commit wireless
wifi up radio0

end=`date +%s`
echo WiFi enabled after `expr $end - $start` seconds. > /dev/console

exit 0
