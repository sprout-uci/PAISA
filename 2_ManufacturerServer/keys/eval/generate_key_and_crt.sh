# /bin/bash
# key: secp256r1 == prime256v1

dev_dir=dev
ttp_dir=ttp
dev_crt_conf=dev_crt.cnf
dev_csr_conf=dev_csr.cnf
ttp_crt_conf=ttp_crt.cnf

# Execute the following commands
rm -rf $secp256_dir
rm -rf $secp384_dir
rm -rf $secp521_dir

if [ $# -gt 0 ]
then
	# Check if certificates generation successfully done
	if [ ! -d "secp256r1" ] || [ ! -d "secp256r1" ] || [ ! -d "secp256r1" ]; then
		echo "[Usage] generate_key_and_crt.sh"
		exit 1
	fi

	for key_size in 256 384 521
	do
		echo "================================ [${key_size}] Certificate of TTP ================================"
		openssl x509 -noout -text -in secp${key_size}r1/$ttp_dir/crt.pem
		
		echo "================================ [${key_size}] CSR of Device ================================"
		openssl req -noout -text -in secp${key_size}r1/$dev_dir/csr.pem
		
		echo "================================ [${key_size}] Certificate of Device ================================"
		openssl x509 -noout -text -in secp${key_size}r1/$dev_dir/crt.pem
	done
	exit 0
fi

for key_size in 256 384 521
do
	mkdir -p secp${key_size}r1/$dev_dir
	mkdir -p secp${key_size}r1/$ttp_dir

	cp $dev_crt_conf secp${key_size}r1/
	cp $dev_csr_conf secp${key_size}r1/
	cp $ttp_crt_conf secp${key_size}r1/

	if [ "$key_size" != "256" ]; then
		sed -i "s/prime256v1/secp${key_size}r1/g" secp${key_size}r1/$dev_crt_conf
		sed -i "s/256/${key_size}/g" secp${key_size}r1/$dev_crt_conf
		sed -i "s/prime256v1/secp${key_size}r1/g" secp${key_size}r1/$dev_csr_conf
		sed -i "s/256/${key_size}/g" secp${key_size}r1/$dev_csr_conf
		sed -i "s/prime256v1/secp${key_size}r1/g" secp${key_size}r1/$ttp_crt_conf
		sed -i "s/256/${key_size}/g" secp${key_size}r1/$ttp_crt_conf
	fi

	# Dev side
	if [ "$key_size" == "256" ]; then
		openssl ecparam -name prime256v1 -genkey -out secp${key_size}r1/$dev_dir/key.pem
	else
		openssl ecparam -name secp${key_size}r1 -genkey -out secp${key_size}r1/$dev_dir/key.pem
	fi
	openssl ec -in secp${key_size}r1/$dev_dir/key.pem -pubout > secp${key_size}r1/$dev_dir/pub.pem
	openssl req -new -key secp${key_size}r1/$dev_dir/key.pem -out secp${key_size}r1/$dev_dir/csr.pem -config dev_csr.cnf

	# TTP side
	if [ "$key_size" == "256" ]; then
		openssl ecparam -name prime256v1 -genkey -out secp${key_size}r1/$ttp_dir/key.pem
	else
		openssl ecparam -name secp${key_size}r1 -genkey -out secp${key_size}r1/$ttp_dir/key.pem
	fi
	openssl ec -in secp${key_size}r1/$ttp_dir/key.pem -pubout > secp${key_size}r1/$ttp_dir/pub.pem
	openssl req -x509 -new -key secp${key_size}r1/$ttp_dir/key.pem -out secp${key_size}r1/$ttp_dir/crt.pem -days 365 -config ttp_crt.cnf 

	openssl x509 -req -in secp${key_size}r1/$dev_dir/csr.pem -out secp${key_size}r1/$dev_dir/crt.pem -days 365 -CA secp${key_size}r1/$ttp_dir/crt.pem -CAkey secp${key_size}r1/$ttp_dir/key.pem -CAcreateserial -extensions req_ext -extfile dev_crt.cnf

done

