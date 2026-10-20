# SPDX-License-Identifier: MIT
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('incremental', Path(__file__).resolve().parents[1] / 'incremental.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class SourceSyncTests(unittest.TestCase):
    def test_changed_deleted_unchanged_and_disposable_objects(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source'; source.mkdir()
            work = Path(directory) / 'work'; work.mkdir()
            (source / 'same.c').write_text('unchanged')
            (source / 'changed.c').write_text('new')
            (work / 'same.c').write_text('unchanged')
            before = (work / 'same.c').stat().st_mtime_ns
            (work / 'changed.c').write_text('old')
            (work / 'deleted.c').write_text('removed')
            (work / 'generated.o').write_text('cached object')
            names = module.sync_source(source, work, ['same.c', 'changed.c', 'deleted.c'])
            self.assertEqual(names, ['changed.c', 'same.c'])
            self.assertEqual((work / 'same.c').stat().st_mtime_ns, before)
            self.assertEqual((work / 'changed.c').read_text(), 'new')
            self.assertFalse((work / 'deleted.c').exists())
            self.assertTrue((work / 'generated.o').exists())

    def test_manifest_cannot_delete_outside_work(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source'; source.mkdir()
            work = Path(directory) / 'work'; work.mkdir()
            victim = Path(directory) / 'keep.c'; victim.write_text('keep')
            with self.assertRaises(ValueError):
                module.sync_source(source, work, ['../keep.c'])
            self.assertTrue(victim.exists())

    def test_directory_symlink_and_executable_mode_are_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source'; source.mkdir()
            work = Path(directory) / 'work'; work.mkdir()
            (source / 'include').symlink_to('headers', target_is_directory=True)
            (source / 'tool').write_text('tool'); (source / 'tool').chmod(0o755)
            (work / 'tool').write_text('tool'); (work / 'tool').chmod(0o644)
            module.sync_source(source, work, ['tool'])
            self.assertEqual((work / 'include').readlink(), Path('headers'))
            self.assertEqual((work / 'tool').stat().st_mode & 0o777, 0o755)


if __name__ == '__main__':
    unittest.main()
