"""Offline RTL provenance/import checks; no network or private fixtures."""
from pathlib import Path
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]

class PrepareRtlTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);(self.root/'tools').mkdir();(self.root/'docs').mkdir()
        shutil.copy2(ROOT/'tools/prepare_rtl.py',self.root/'tools/prepare_rtl.py')
        shutil.copy2(ROOT/'docs/rtl_provenance.json',self.root/'docs/rtl_provenance.json')
    def run_tool(self,*args,success=True):
        run=subprocess.run([sys.executable,str(self.root/'tools/prepare_rtl.py'),*map(str,args)],capture_output=True,text=True)
        self.assertEqual(run.returncode==0,success,run.stdout+run.stderr);return run
    def stage_upstream(self):
        source=self.root/'upstream';shutil.copytree(ROOT/'rtl/src',source/'src');shutil.copy2(ROOT/'rtl/IKAOPM_LICENSE',source/'LICENSE');return source
    def test_verified_import_check_and_explicit_repair(self):
        upstream=self.stage_upstream()
        self.run_tool('--from-dir',upstream);self.run_tool('--check')
        target=self.root/'rtl/src/IKAOPM.v';original=target.read_bytes();target.write_bytes(b'changed locally')
        self.run_tool('--check',success=False);self.run_tool('--from-dir',upstream,success=False)
        self.assertEqual(target.read_bytes(),b'changed locally')
        self.run_tool('--from-dir',upstream,'--force');self.assertEqual(target.read_bytes(),original)
        self.run_tool('--check')
    def test_bad_source_fails_before_any_publication(self):
        source=self.root/'source';shutil.copytree(ROOT/'rtl/src',source/'src');shutil.copy2(ROOT/'rtl/IKAOPM_LICENSE',source/'LICENSE')
        (source/'src/IKAOPM.v').write_bytes(b'not the pinned RTL')
        self.run_tool('--from-dir',source,success=False);self.assertFalse((self.root/'rtl').exists())
    def test_missing_check_does_not_prepare_or_fetch(self):
        self.run_tool('--check',success=False);self.assertFalse((self.root/'rtl').exists())
    def test_foreign_remote_is_rejected_without_output(self):
        self.run_tool('--fetch-raw','https://example.com/foreign',success=False)
        self.assertFalse((self.root/'rtl').exists())

if __name__=='__main__':unittest.main(verbosity=2)
