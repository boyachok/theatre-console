import unittest,struct,zlib,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from helmet_usb_update import active_slot,SIZE

def flash(seq=None,modern=False):
 b=bytearray(b'\xff'*SIZE);size=0x600000 if modern else 0x140000;second=0x10000+size
 entries=[(1,2,0x9000,0x5000,b'nvs'),(1,0,0xe000,0x2000,b'otadata'),(0,0x10,0x10000,size,b'app0'),(0,0x11,second,size,b'app1'),(1,0x82,second+size,0x100000,b'spiffs')]
 if modern:entries[-1]=(1,0x82,0xc10000,0x3f0000,b'storage')
 for i,(kind,sub,offset,n,label)in enumerate(entries):b[0x8000+32*i:0x8020+32*i]=struct.pack('<HBBII16sI',0x50aa,kind,sub,offset,n,label,0)
 if seq:
  row=bytearray(b'\xff'*32);struct.pack_into('<I',row,0,seq);struct.pack_into('<I',row,24,2);struct.pack_into('<I',row,28,zlib.crc32(row[:4],0xffffffff));b[0xe000:0xe020]=row
 return b
class Layout(unittest.TestCase):
 def test_preserved_small_slots(self):
  self.assertEqual(active_slot(flash(1))['offset'],0x10000);self.assertEqual(active_slot(flash(2))['offset'],0x150000);self.assertEqual(active_slot(flash(2))['size'],0x140000)
 def test_modern_and_blank(self):
  self.assertEqual(active_slot(flash(8,True))['offset'],0x610000);self.assertEqual(active_slot(flash())['offset'],0x10000)
 def test_crc_and_overlap_rejected(self):
  b=flash(1);b[0xe000]=5
  with self.assertRaises(ValueError):active_slot(b)
  b=flash(1);struct.pack_into('<I',b,0x8000+3*32+4,0x10000)
  with self.assertRaises(ValueError):active_slot(b)
 def test_nonstandard_nvs_and_missing_slot_rejected(self):
  b=flash(1);struct.pack_into('<I',b,0x8000+4,0x7000)
  with self.assertRaises(ValueError):active_slot(b)
  b=flash(1);b[0x8060:0x8080]=b'\xff'*32
  with self.assertRaises(ValueError):active_slot(b)
if __name__=='__main__':unittest.main()
