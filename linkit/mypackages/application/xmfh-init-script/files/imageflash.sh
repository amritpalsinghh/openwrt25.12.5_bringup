#! /bin/sh
# Not an OpenWrt init script. Do not install under /etc/init.d/
# (apk postinst would run this on install: fw_setenv / mtd write).
# Kept for later; bringup start() does not call it.

GOLDEN_IMAGE=/root/MCU/lks7688_G.img
U_BOOT_IMAGE=/root/MCU/uboot.bin
UPDATE_REF_VAL=/root/MCU/update_ref_val.py
REF_TABLE=/root/MCU/referenceTable.CSV

start=`date +%s`

timeout=3

while [ $timeout -gt 0 ]
do
	timeout=$(($timeout-1))
	sleep 1
done

echo "*****UBoot and GoldenImage*****" > /dev/console
fw_setenv boot_success 1

str1=$(fw_printenv boot_loaded)
str2=$(fw_printenv gold_loaded)
str3=$(fw_printenv conf_loaded)

if [[ $str1 == "boot_loaded=v9.0.8.5" ]]
then
	echo "U-boot is upto date" > /dev/console
	rm $U_BOOT_IMAGE

else
        echo "Updating U-Boot....!!" > /dev/console
        fw_setenv boot_loaded v9.0.8.5
	mtd -r write $U_BOOT_IMAGE u-boot
        rm $U_BOOT_IMAGE
fi

if [[ $str2 == "gold_loaded=v0.0.0.6" ]]
then
        echo "Golden Image is upto date" > /dev/console
        rm $GOLDEN_IMAGE

else
        echo "Updating Golden Image....!!" > /dev/console
        fw_setenv gold_loaded v0.0.0.6
        echo -e "y\n" | jffs2reset
        mtd -r write $GOLDEN_IMAGE golden-image
        rm $GOLDEN_IMAGE
        reboot -f
fi

if [[ $str3 == "conf_loaded=1" ]]
then
	echo "Config is upto date" > /dev/console
	rm $UPDATE_REF_VAL $REF_TABLE

else
        echo "Updating Config....!!" > /dev/console
        python $UPDATE_REF_VAL 
	fw_setenv conf_loaded 1       
        rm $UPDATE_REF_VAL $REF_TABLE
        
fi

end=`date +%s`
echo *************END*************** `expr $end - $start` seconds. > /dev/console

exit 0
