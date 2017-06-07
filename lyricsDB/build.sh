mkdir -p bin
cd src


for d in net protocol common server client; do
	cd "$d"
	printf "building %s... " "$(basename "$d")"
	make 1>/dev/null
	if [ $? -ne "0" ]; then
		printf "FAILED\n"
	else
		printf "OK\n"
	fi
	cd ..
done
