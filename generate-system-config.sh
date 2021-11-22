#!/bin/bash

SYS_CONFFILE="/var/lib/wb-mqtt-adc/conf.d/system.conf"

if [[ -d /sys/firmware/devicetree/base/wirenboard/analog-inputs ]]; then
	source "/usr/lib/wb-utils/common.sh"
	wb_source "of"
	wb_source "wb_env_of"
	WB_ENV_CACHE="${WB_ENV_CACHE:-/var/run/wb_env.cache}"

	declare -A OF_IIODEV_LINKS
	of_find_iio_dev_links

	WB_OF_ROOT="/wirenboard"
	node="${WB_OF_ROOT}/analog-inputs"

	ADCSYSCONF='{\n "iio_channels": ['
	first=1
	for ch_name in  $(of_node_children "$node" | sort); do
		phandle=$(of_get_prop_ulong "$node/$ch_name" "iio-device")

		if $(of_has_prop "$node/$ch_name" "iio-channel-name"); then
			iio_channel_name=$(of_get_prop_str "$node/$ch_name" "iio-channel-name")
		else
			iio_channel_name="voltage0"
		fi

		if $(of_has_prop "$node/$ch_name" "divider-r1-ohms"); then
			divider_r1=$(of_get_prop_ulong "$node/$ch_name" "divider-r1-ohms")
			divider_r2=$(of_get_prop_ulong "$node/$ch_name" "divider-r2-ohms")
			multiplier=$(echo "scale=5; ($divider_r1 + $divider_r2) / $divider_r2" | bc)
		else
			multiplier="1"
		fi

		ITEM=" { \
			\"match_iio\" : \"${OF_IIODEV_LINKS[$phandle]}\", \
			\"id\" : \"$ch_name\", \
			\"channel_number\" : \"$iio_channel_name\", \
			\"voltage_multiplier\" : $multiplier ,"

		if $(of_has_prop "$node/$ch_name" "averaging-window"); then
			averaging_window=$(of_get_prop_ulong "$node/$ch_name" "averaging-window")
			ITEM="$ITEM\"averaging_window\" : $averaging_window,"
		fi

		if $(of_has_prop "$node/$ch_name" "decimal-places"); then
			ITEM="$ITEM\"decimal_places\" : $(of_get_prop_ulong "$node/$ch_name" "decimal-places"),"
		fi

		ITEM="$ITEM \
			\"order\" : $(of_has_prop "$node/$ch_name" "sort-order" && echo -n $(of_get_prop_ulong "$node/$ch_name" "sort-order") || echo -n 0) \
		}"

		if (( first )); then
			first=0
		else
			ADCSYSCONF="$ADCSYSCONF,"
		fi

		ADCSYSCONF="$ADCSYSCONF\n$ITEM"
	done
	ADCSYSCONF="$ADCSYSCONF\n]}"
	echo -e $ADCSYSCONF | jq '.iio_channels|=sort_by(.order)|.iio_channels|=map(del(.order))' > ${SYS_CONFFILE}
else
	echo "/sys/firmware/devicetree/base/wirenboard/analog-inputs is missing"
fi
