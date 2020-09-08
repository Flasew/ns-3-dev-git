#!/bin/bash
rm -f pdata
touch pdata
for i in $( ls ); do
  if [[ $i == "config"* ]]; then
    echo -n "${i: -19}" >> pdata
    echo -ne "\t" >> pdata
    echo -n `sed '8q;d' $i | cut -d, -f2` >> pdata
    echo -ne "\t" >> pdata
    echo -n `sed '7q;d' $i | cut -d' ' -f2` >> pdata
    echo -ne "\t" >> pdata
    echo -n `sed '5q;d' $i | cut -d' ' -f2 | cut -dp -f1` >> pdata
    echo -ne "\t" >> pdata
    echo -n `sed '3q;d' $i | cut -d' ' -f2` >> pdata
    echo -ne "\t" >> pdata
    echo -n `sed '13q;d' $i | cut -d' ' -f2` >> pdata
    echo -ne "\t" >> pdata
    for j in $( ls ); do
      if [[ $j == "drlog"* && "${i: -19}" == "${j: -19}" ]]; then
        echo drlog_"${j: -19}"
        if [ -s drlog_"${j: -19}" ]; then
          echo -n `tail -n 1 drlog_"${j: -19}" | cut -d, -f2` >> pdata
        else
          echo -n "0" >> pdata
        fi
      fi
    done
    echo -ne "\t" >> pdata
    echo -n `sed '11q;d' $i | cut -d' ' -f2` >> pdata
    echo -ne "\t" >> pdata
    echo    `sed '12q;d' $i | cut -d' ' -f2` >> pdata
  fi
done
