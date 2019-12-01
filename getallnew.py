import os
import json
import xml.etree.ElementTree as ET
import pandas as pd 

def getmeanrtt(root, dport):
    senderflows = set([f.attrib["flowId"] for f in root[1] if f.attrib["destinationPort"]==str(dport)])
    flowmap = dict([(s.attrib["flowId"], s.attrib["sourcePort"] if s.attrib["flowId"] in senderflows else s.attrib["destinationPort"]) for s in root[1]])
    rtts = dict([(s.attrib["sourcePort"], 0) for s in root[1] if s.attrib["destinationPort"]==str(dport)])
    for f in root[0]:
        srcport = flowmap[f.attrib["flowId"]]
        rtts[srcport] += int((float(f.attrib["delaySum"][:-2])/1000)/float(f.attrib["rxPackets"]))
    return sum(rtts.values())/len(rtts)

def getretrans(root, dport):
    senderflows = set([f.attrib["flowId"] for f in root[1] if f.attrib["destinationPort"]==str(dport)])
    rsum = 0
    for f in root[0]:
        if f.attrib["flowId"] in senderflows:
            rsum += int(f.attrib["lostPackets"])
    return rsum

pdataname = "pdata"
with open(pdataname, "w") as pdata:
    for jname in [fname for fname in os.listdir(os.getcwd()) if fname[-4:]=="json"]:
        print(jname)
        tstamp = jname[7:-5]
        xname = "mon_" + tstamp + ".xml"
        xroot = None
        try:
            with open(jname, "r") as j: jcontent = j.read()
            xroot = ET.parse(xname).getroot()
        except:
            continue
        pdata.write(tstamp + "\t")
        jdict = json.loads(jcontent)
        pdata.write(str(jdict["bwp"][0]["period"]) + "\t")
        pdata.write(str(jdict["nflows"]) + "\t")
        qlen = int(jdict["queue_length"])
        pdata.write(str(qlen) + "\t")
        delay = 2*int(jdict["bwp"][0]["delay"][:-2])
        pdata.write(str(delay) + "\t")
        fct = max([int(float(f.attrib["timeLastRxPacket"][:-2])/1000) for f in xroot[0]])
        pdata.write(str(fct) + "\t")
        retransmits = getretrans(xroot, 2048)
        pdata.write(str(retransmits) + "\t")
        # rto = sum([r["timeout"] for r in jdict["flowdata"]])
        # pdata.write(str(rto) + "\t")
        csfname = "congStatelog_" + tstamp
        csdata = pd.read_csv(csfname, header=None, index_col=None, names=['tstamp', 'flowid', 'subflow', 'csfrom', 'csto'])
        rto = csdata[csdata["csto"]==4].shape[0]
        pdata.write(str(rto) + "\t")
        torec = csdata[csdata["csto"]==3].shape[0]
        pdata.write(str(torec) + "\t")
        syn = sum([r["syn_sent"] for r in jdict["flowdata"]])
        pdata.write(str(syn) + "\t")
        meanrtt = getmeanrtt(xroot, 2048)
        pdata.write(str(meanrtt) + "\n")


