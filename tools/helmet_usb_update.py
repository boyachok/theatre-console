#!/usr/bin/env python3
"""Install helmet app ONLY, preserving the existing partition table, OTA selection, NVS and FS.
First OTA-capable install needs USB. Requires explicit target MAC; never erases flash.
"""
import argparse, datetime, hashlib, json, os, re, struct, subprocess, time, zlib
from pathlib import Path

MARKER = b'THEATRE_HELMET_OTA_ROLE_V1'
SIZE = 0x1000000

def active_slot(flash):
    if len(flash) != SIZE:
        raise ValueError('Expected full16MB backup')
    parts=[]
    for at in range(0x8000,0x8c00,32):
        row=flash[at:at+32]
        if row[:2] == b'\xeb\xeb':
            if row[16:32] != hashlib.md5(flash[0x8000:at]).digest():raise ValueError('Partition-table MD5 mismatch')
            break
        if row[:2] != b'\xaaP':
            if row != b'\xff'*32:raise ValueError('Invalid partition-table entry')
            break
        _,kind,sub,offset,size,label,_=struct.unpack('<HBBII16sI',row)
        if not size or offset+size>SIZE: raise ValueError('Invalid partition bounds')
        parts.append(dict(kind=kind,sub=sub,offset=offset,size=size,label=label.split(b'\0')[0].decode('ascii')))
    ordered=sorted(parts,key=lambda p:p['offset'])
    if any(a['offset']+a['size']>b['offset']for a,b in zip(ordered,ordered[1:])):raise ValueError('Overlapping partitions')
    nvs=next((p for p in parts if p['label']=='nvs'),None)
    if not nvs or (nvs['offset'],nvs['size'])!=(0x9000,0x5000):raise ValueError('Nonstandard NVS layout: inspect manually')
    apps=sorted((p for p in parts if p['kind']==0 and 0x10<=p['sub']<=0x1f),key=lambda p:p['sub'])
    if len(apps)!=2 or [p['sub']for p in apps]!=[0x10,0x11] or any(p['offset']%0x10000 or p['offset']<0x10000 or p['size']%0x1000 for p in apps):
        raise ValueError('Need an existing two-slot OTA layout; partition replacement is NOT allowed')
    ota=next((p for p in parts if p['kind']==1 and p['sub']==0),None)
    if not ota or (ota['offset'],ota['size'])!=(0xe000,0x2000): raise ValueError('Nonstandard OTAdata partition')
    seqs=[]
    for at in [ota['offset'],ota['offset']+0x1000]:
        entry=flash[at:at+32];seq=int.from_bytes(entry[:4],'little');state,crc=struct.unpack_from('<II',entry,24)
        if seq not in (0,0xffffffff) and state in (0,1,2,0xffffffff) and zlib.crc32(entry[:4],0xffffffff)==crc:seqs.append(seq)
    if not seqs:
        if flash[ota['offset']:ota['offset']+ota['size']] != b'\xff'*ota['size']:
            raise ValueError('Invalid OTAdata: inspect before overwriting either application')
        return apps[0]
    if max(seqs)-min(seqs)>0x7fffffff:raise ValueError('OTA sequence wrap: manual inspection required')
    return apps[(max(seqs)-1)%len(apps)]

def serial_status(port, seconds=2):
    import serial
    data=b'';state=None
    with serial.Serial(port,115200,timeout=.1) as s:
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            s.write(b'j\n');until=min(end,time.monotonic()+.4)
            while time.monotonic()<until:data+=s.read(30000)
        for line in data.decode(errors='replace').splitlines():
            try:
                q=json.loads(line)
                if isinstance(q,dict)and q.get('role'):state=q
            except ValueError:pass
    return data,state

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True);parser.add_argument('--image',type=Path,required=True)
    parser.add_argument('--expected-mac',required=True);parser.add_argument('--version',required=True)
    parser.add_argument('--esptool',type=Path,default=Path.home()/'Library/Arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool')
    args=parser.parse_args();os.umask(0o077);mac=args.expected_mac.upper()
    if not re.fullmatch(r'(?:[0-9A-F]{2}:){5}[0-9A-F]{2}',mac):raise ValueError('Invalid expected MAC')
    if mac in ['A0:85:E3:E2:BC:D4','84:FC:E6:66:F3:8C','30:30:F9:1B:FD:6C']:raise ValueError('Known master/remote: refusing helmet replacement')
    from serial.tools import list_ports
    for device in list_ports.comports():
        if device.device==args.port and device.serial_number and re.fullmatch(r'(?:[a-fA-F0-9]{2}:){5}[a-fA-F0-9]{2}',device.serial_number):
            if device.serial_number.upper()!=mac:raise ValueError('USB descriptor MAC mismatch; refusing even a reset')
    image=args.image.read_bytes()
    if len(image)<256 or image[:1]!=b'\xe9' or image[12:14]!=b'\x09\x00' or MARKER not in image or (args.version+'\0').encode()not in image:raise ValueError('Wrong helmet/S3/version image')
    root=Path.home()/'Theatre-console-backups'/('usb-helmet-ota-'+mac.replace(':','')+'-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));root.mkdir(mode=0o700,parents=True)
    def save(name,data):(root/name).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
    def run(command,name,after='no-reset'):
        q=subprocess.run([str(args.esptool),'--chip','esp32s3','--port',args.port,'--baud','460800','--before','usb-reset','--after',after]+command,capture_output=True,text=True)
        (root/name).write_text(q.stdout+q.stderr)
        if q.returncode:raise RuntimeError('esptool failed; see '+str(root/name))
        return q.stdout
    def identify(name):
        text=run(['read-mac'],name);found={v.upper()for v in re.findall(r'(?:[a-fA-F0-9]{2}:){5}[a-fA-F0-9]{2}',text)}
        if found!={mac}:raise ValueError('Target identity mismatch; no write')
    raw,before=serial_status(args.port,3);(root/'serial-before.log').write_bytes(raw)
    if before:
        if before.get('role')!='helmet' or before.get('mac','').upper()!=mac:raise ValueError('Runtime role/MAC mismatch')
        if before.get('defaultsSaved')is False:raise ValueError('Startup settings are not saved; no write')
        save('status-before.json',before)
        import serial
        with serial.Serial(args.port,115200,timeout=.1)as s:s.write(b'!\n');time.sleep(.25)
    identify('identity.log');print('Backing up full16MB:',root,flush=True)
    run(['read-flash','0',hex(SIZE),str(root/'flash-before-16MB.bin')],'backup.log')
    flash=(root/'flash-before-16MB.bin').read_bytes();slot=active_slot(flash)
    if len(image)>slot['size']:raise ValueError('Image exceeds preserved active slot; no write')
    save('before-write.json',{'mac':mac,'role':'helmet','version':args.version,'slot':slot,'imageSha256':hashlib.sha256(image).hexdigest(),'fullBackupSha256':hashlib.sha256(flash).hexdigest(),'applicationOnly':True})
    identify('identity-before-write.log');run(['write-flash',hex(slot['offset']),str(args.image)],'write.log')
    verified=run(['verify-flash',hex(slot['offset']),str(args.image)],'verify.log')
    if 'Verification successful'not in verified:raise RuntimeError('Image digest not confirmed')
    run(['read-flash','0','0x10000',str(root/'prefix-after.bin')],'prefix-check.log','watchdog-reset')
    if (root/'prefix-after.bin').read_bytes()!=flash[:0x10000]:raise RuntimeError('Boot/table/NVS/OTAdata changed: inspect, do not retry')
    state=None
    for _ in range(12):
        time.sleep(1)
        try:raw,state=serial_status(args.port,1)
        except Exception:continue
        (root/'serial-after.log').write_bytes(raw)
        if state and state.get('mac','').upper()==mac and state.get('role')=='helmet'and state.get('version')==args.version and state.get('otaSupported') and state.get('appAddress')==slot['offset']:
            if before and before.get('token')and state.get('token')==before['token']:continue
            break
    else:raise RuntimeError('App written, but new boot/OTA not confirmed; see '+str(root))
    if before and before.get('voiceAvailable')and not state.get('voiceAvailable'):raise RuntimeError('New app/OTA booted but voice initialization failed; inspect before use')
    save('status-after.json',state);save('result.json',{'mac':mac,'version':args.version,'appSha256':hashlib.sha256(image).hexdigest(),'newBootAndOtaVerified':True,'bootloaderTableNvsOtadataByteIdentical':True,'filesystemNotWritten':True,'slot':slot})
    print('Verified helmet',mac,args.version,'OTA available. Preserved layout/NVS/FS. Evidence:',root)

if __name__=='__main__':main()
