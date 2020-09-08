#!/bin/bash
rep=1
data=4000000
TAB='\t'
tout=64
exitid=0
rwnd=524288
dlscale=5
rate1=1
rate2=0.1
presend=5
#declare -a flarr=(5 10 15 30)
#declare -a qlarr=(100 200 400 800)
#declare -a dlarr=(50 100 200 500 1000 2000)
declare -a bwarr=(128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576)
#declare -a bwarr=(64 90 128 181 256 362 512 724 1024 1448 2048 2896 4096 5792 8192 11585 16384 23170 32768 46340 65536 92681 131072 185363 262144 370727 524288 741455 1058576)
declare -a flarr=(5 10)
declare -a qlarr=(100)
declare -a dlarr=(100)
#declare -a bwarr=(4096)
for b in "${bwarr[@]}"; do
  for q in "${qlarr[@]}"; do
    for d in "${dlarr[@]}"; do
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --stop

      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --ch 0 --qslf ${q} --qslb $((q*1514))
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --ch 4 --qslf ${q} --qslb $((q*1514))

      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 0 --dwell $((156*(b-presend)))
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 2 --dwell $((156*(b-presend)))
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 1 --dwell $((156*(presend)))
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 3 --dwell $((156*(presend)))

      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 0 --ch 0 --rate ${rate2} --delay 0
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 0 --ch 4 --rate ${rate2} --delay 0
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 0 --ch 7 6 --delay $((156*d)) --rate 1
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 0 --ch 3 1 --delay $((156*d*dlscale)) --rate 1

      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 1 --ch 0 --rate ${rate2} --delay 0
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 1 --ch 4 --rate ${rate2} --delay 0
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 1 --ch 7 6 --delay $((156*d)) --rate 1
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 1 --ch 3 1 --delay $((156*d*dlscale)) --rate 1

      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 2 --ch 0 --rate ${rate1} --delay 0
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 2 --ch 4 --rate ${rate1} --delay 0
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 2 --ch 7 6 --delay $((156*d)) --rate 1
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 2 --ch 3 1 --delay $((156*d*dlscale)) --rate 1

      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 3 --ch 0 --rate ${rate1} --delay 0
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 3 --ch 4 --rate ${rate1} --delay 0
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 3 --ch 7 6 --delay $((156*d)) --rate 1
      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --config 3 --ch 3 1 --delay $((156*d*dlscale)) --rate 1

      python3 /home/wew168/tc-fpga-0.3/tc_config.py -p /dev/ttyUSB1 --run

      for j in "${flarr[@]}"; do
        n=0
        while [ $n -lt $rep ]; do
          n=$((n+1))
          fname=log_$(eval "date +%m_%d_%Y_%H_%M_%S").json
          ssh root@b09-06 "timeout ${tout}s /root/tdtcp-mtcp/apps/example/perfclient 10.1.0.4/index.html $j -N 1 -f /root/tdtcp-mtcp/apps/example/epwget.conf -p ${data}" > $fname
          exitid=`echo $?`
          echo $exitid
          if [ ${exitid} -ne 0 ]; then
            printf "{\n\n" > $fname
          fi
          sed -i "2i\ \"delay\":\ $d," $fname
          sed -i "2i\ \"qlen\":\  $q," $fname
          sed -i "2i\ \"bw_freq\":\ $b," $fname
          sed -i "2i\ \"nflows\":\  $j," $fname
          if [ ${exitid} -ne 0 ]; then
            printf " \"streams\": []\n" >> $fname
            printf "}" >> $fname
          fi
          exitid=0
        done
      done
    done
  done
done
      #python3 tc_config.py -p /dev/ttyUSB1 --config 0 --ch 0 --rate 0.55 --delay 156000 --dwell 1650000 --pkt "00 02 C9 45 26 01 00 00 00 00 00 00 08 00 45 00 00 1C 00 00 00 00 40 01 00 00 0A 01 00 05 0A 01 00 06 7B 00 00 00 00 00 00 00"
      #python3 tc_config.py -p /dev/ttyUSB1 --config 1 --ch 0 --rate 0.55 --delay 780000 --dwell 1650000 --pkt "00 02 C9 45 26 01 00 00 00 00 00 00 08 00 45 00 00 1C 00 00 00 00 40 01 00 00 0A 01 00 05 0A 01 00 06 7B 00 00 00 00 00 00 01"
      #--pkt "00 02 C9 45 25 E1 00 00 00 00 00 00 08 00 45 00 00 1C 00 00 00 00 40 01 00 00 0A 01 00 05 0A 01 00 04 7B 00 00 00 00 00 00 01"
      #--pkt "00 02 C9 45 25 E1 00 00 00 00 00 00 08 00 45 00 00 1C 00 00 00 00 40 01 00 00 0A 01 00 05 0A 01 00 04 7B 00 00 00 00 00 00 00"
