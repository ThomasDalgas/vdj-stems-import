# SPDX-License-Identifier: GPL-3.0-only
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
BIN = Path(sys.argv.pop(1)).resolve()
class ScanTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)/'Contents'; self.root.mkdir()
    def track(self, name='Artist/Album/Æ Song "mix".mp3'):
        p=self.root/name; p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(b'original')
        d=Path(str(p)+'.stems'); d.mkdir()
        for stem in ('Vocals','Instruments','Bass','Drums'): (d/(stem+'.m4a')).write_bytes(b'fixture, not audio')
        return p,d
    def scan(self, root=None):
        r=subprocess.run([str(BIN),str(root or self.root)],capture_output=True,text=True,check=False)
        return r.returncode,[json.loads(s) for s in r.stdout.splitlines()]
    def test_complete_unicode_and_json_escaping(self):
        p,d=self.track(); code,rows=self.scan()
        self.assertEqual(code,0); self.assertEqual(rows[1]['original'],str(p.relative_to(self.root)))
        self.assertTrue(rows[1]['files_complete']); self.assertFalse(rows[1]['playback_ready'])
        self.assertFalse(rows[0]['codec_verified'])
    def test_exact_azmod_slot_order(self):
        p,d=self.track(); code,rows=self.scan()
        self.assertEqual(rows[0]['version'],2)
        self.assertEqual(rows[0]['groups'],['Vocals','Instruments','Bass','Drums'])
        self.assertEqual(rows[0]['azmod_slots'],['VOCAL','MELODY','BASS','DRUMS'])
        for i,name in enumerate(rows[0]['groups']):
            f=d/(name+'.m4a'); f.unlink()
            code,result=self.scan()
            self.assertEqual(result[1]['present_mask'],15 ^ (1 << i))
            self.assertEqual(code,4)
            f.write_bytes(b'fixture')
    def test_old_overlapping_groups_are_not_mapped(self):
        p,d=self.track()
        for f in d.iterdir(): f.unlink()
        for name in ('Acapella','Instrumental','Melody','Beat'):
            (d/(name+'.m4a')).write_bytes(b'legacy')
        code,rows=self.scan()
        self.assertEqual(code,4)
        self.assertEqual(rows[1]['present_mask'],0)
        self.assertFalse(rows[1]['files_complete'])
    def test_no_instrumental_alias_for_instruments(self):
        p,d=self.track(); (d/'Instruments.m4a').rename(d/'Instrumental.m4a')
        code,rows=self.scan()
        self.assertEqual(code,4)
        self.assertEqual(rows[1]['present_mask'],13)
    def test_missing_and_empty(self):
        p,d=self.track(); (d/'Drums.m4a').unlink(); (d/'Bass.m4a').write_bytes(b'')
        code,rows=self.scan(); self.assertEqual(code,4); self.assertEqual(rows[1]['invalid_mask'],4)
        self.assertEqual(rows[1]['present_mask'],3)
    def test_orphan(self):
        p,d=self.track(); p.unlink(); code,rows=self.scan()
        self.assertEqual(code,4); self.assertFalse(rows[1]['original_exists'])
    def test_symlink_group_not_accepted(self):
        p,d=self.track(); f=d/'Drums.m4a'; f.unlink(); f.symlink_to(p)
        code,rows=self.scan(); self.assertEqual(code,4); self.assertEqual(rows[1]['invalid_mask'],8)
    def test_symlink_directory_not_followed(self):
        (self.root/'escape').symlink_to(self.root, target_is_directory=True)
        code,rows=self.scan(); self.assertEqual(code,0); self.assertEqual(rows[-1]['sets'],0)
    def test_symlink_root_rejected(self):
        link=Path(self.tmp.name)/'link'; link.symlink_to(self.root,target_is_directory=True)
        code,rows=self.scan(link); self.assertEqual(code,1)
    def test_case_insensitive_groups_and_apple_sidecars(self):
        p,d=self.track(); (d/'Drums.m4a').rename(d/'drums.M4A'); (d/'._Drums.m4a').write_bytes(b'sidecar')
        code,rows=self.scan(); self.assertEqual(code,0); self.assertEqual(rows[1]['present_mask'],15)
    def test_missing_root(self):
        code,rows=self.scan(self.root/'missing'); self.assertEqual(code,1); self.assertEqual(rows[-1]['errors'],1)
    def test_depth_limit(self):
        p=self.root
        for _ in range(10): p=p/'deep'; p.mkdir()
        code,rows=self.scan(); self.assertEqual(code,2)
    def test_does_not_write_usb(self):
        self.track()
        before={str(p):(p.stat().st_mtime_ns,p.stat().st_size) for p in self.root.rglob('*')}
        self.scan()
        after={str(p):(p.stat().st_mtime_ns,p.stat().st_size) for p in self.root.rglob('*')}
        self.assertEqual(before,after)
if __name__ == '__main__': unittest.main()
