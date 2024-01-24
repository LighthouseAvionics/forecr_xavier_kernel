#!/bin/bash

# -----
#!/bin/bash

get_dtb_filename() {
	# Path to the extlinux.conf file
	file_path="/boot/extlinux/extlinux.conf"

	# Extract the default label
	default_label=$(grep '^DEFAULT' "$file_path" | awk '{print $2}')

	# Flag to indicate if we are in the right label section
	in_section=0

	# Read the file line by line
	while read -r line; do
	    # Check for label section start
	    if [[ $line == LABEL* ]]; then
		label=$(echo "$line" | awk '{print $2}')
		if [[ $label == "$default_label" ]]; then
		    in_section=1
		else
		    in_section=0
		fi
	    fi

	    # If in the correct section, look for the FDT line
	    if [[ $in_section -eq 1 && $line == FDT* ]]; then
		fdt=$(echo "$line" | awk '{print $2}')
		echo "$fdt"
		break
	    fi
	done < "$file_path"
}

# Check if minimum number of arguments provided
if [ "$#" -lt 2 ]; then
    echo "Usage: $0 [local_path] [optional: overwrite|no-overwrite] -- <hostname1_prefix> <hostname1_index1> <hostname1_index2> ... [-- <hostname2_prefix> <hostname2_index1> ...] [-- ...]"
    exit 1
fi

LOCAL_PATH="$1"
OVERWRITE_KERNEL="no"

shift 1

# Check if the third argument is for reboot option
if [ "$1" != "--" ]; then
    if [ "$1" == "overwrite" ]; then
        OVERWRITE_KERNEL="yes"
    elif [ "$1" == "no-overwrite" ]; then
        OVERWRITE_KERNEL="no"
    else
	    echo "Invalid overwrite option: $1. Use 'overwrite', 'no-overwrite', or '--' to skip (functions as no-overwrite)."
        exit 1
    fi
    shift
fi

# Now the next argument must be '--'
if [ "$1" != "--" ]; then
    echo "Error: Missing '--' after reboot option."
    echo "Usage: $0 [local_path] [optional: overwrite|no-overwrite] -- <hostname1_prefix> <hostname1_index1> <hostname1_index2> ... [-- <hostname2_prefix> <hostname2_index1> ...] [-- ...]"
    exit 1
fi
shift

echo -n "Enter the password for the remote machine: "
read -s PASSWORD
echo -e '\n'


# Define the files to copy
FILE1="$LOCAL_PATH/arch/arm64/boot/Image"
FILE2="$LOCAL_PATH/arch/arm64/boot/dts/nvidia/tegra234-p3701-0005-p3737-0000-dsboard-agx.dtb"

# Check if the files exist
if [ ! -f "$FILE1" ] || [ ! -f "$FILE2" ]; then
    echo "One or both files do not exist in the specified directory."
    exit 1
fi

# Parse and copy files to each location
all_success=true
REMOTE_USER="heimdall"
while (( "$#" )); do
    if [ "$1" == "--" ]; then
        shift
        continue
    fi

    prefix=$1
    shift

    while (( "$#" )) && [ "$1" != "--" ]; do
        # Determine the remote directory
	this_address_success=true
	REMOTE_ADDRESS="$REMOTE_USER@$prefix$1"
	if [ "$OVERWRITE_KERNEL" == "yes" ]; then
	    sshpass -p $PASSWORD rsync --rsync-path="sudo rsync" "$FILE1" "$REMOTE_ADDRESS:/boot/Image"

            if [ $? -ne 0 ]; then
                echo "SCP of Image to $REMOTE_ADDRESS failed"
                all_success=false
		this_address_success=false
            fi
	    
	    export -f get_dtb_filename
	    dtb_path=$(sshpass -p $PASSWORD ssh $REMOTE_ADDRESS "$(typeset -f get_dtb_filename); get_dtb_filename")
	    if [ "$dtb_path" == "" ]; then
		dtb_path="/boot/dtb/kernel_tegra234-p3701-0005-p3737-0000.dtb"
		echo "couldn't parse dtb file. assuming it is at $dtb_path"
	    fi
	    echo "DTB is at: $dtb_path"
	    sshpass -p $PASSWORD rsync --rsync-path="sudo rsync" "$FILE2" "$REMOTE_ADDRESS:$dtb_path"
	    if [ $? -ne 0 ]; then
                echo "SCP of Device Tree to $REMOTE_ADDRESS failed"
    		all_success=false
		this_address_success=false
	    fi

	    echo "Rebooting remote machine $REMOTE_ADDRESS..."
    	    sshpass -p $PASSWORD ssh "$REMOTE_ADDRESS" "echo $PASSWORD | sudo -S reboot"
	else
	    REMOTE_LOCATION="$REMOTE_ADDRESS:~/Documents"
	    sshpass -p $PASSWORD rsync "$FILE1" "$REMOTE_LOCATION"

            if [ $? -ne 0 ]; then
                echo "SCP of Image to $REMOTE_LOCATION failed"
                all_success=false
		this_address_success=false
            fi
	    
	    sshpass -p $PASSWORD rsync "$FILE2" "$REMOTE_LOCATION"
            if [ $? -ne 0 ]; then
                echo "SCP of Device Tree to $REMOTE_LOCATION failed"
                all_success=false
		this_address_success=false
            fi
	fi
	
	echo "$REMOTE_ADDRESS was copied to successfully: $this_address_success"
        shift
	echo -e '\n'
    done

done


# Check if any scp commands failed
if [ "$all_success" = true ]; then
    echo "All SCP operations were successful"
else
    echo "One or more SCP operations failed. Go check which ones >:("
fi

unset PASSWORD

echo "File transfer complete."
