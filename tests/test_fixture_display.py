"""Portable fake-display contract tests; does not invoke Win32."""
import ctypes
import unittest
from types import SimpleNamespace
from fixture_display import DevMode, allowed, candidates, signature, temporary_mode


def mode(w, h, freq=60, bpp=32, flags=0):
    return SimpleNamespace(width=w, height=h, frequency=freq, bpp=bpp, flags=flags)


class FakeDisplay:
    def __init__(self, choices=(), original=None, reject_test=False, reject_restore=False, single=True):
        self.original = original or mode(1024, 768)
        self.active = self.original
        self.choices = choices
        self.calls = []
        self.reject_test = reject_test
        self.reject_restore = reject_restore
        self.single = single

    def current(self):
        return self.active

    def modes(self):
        return self.choices

    def single_monitor(self):
        return self.single

    def change(self, selected, test=False):
        self.calls.append((signature(selected), test))
        if test:
            return -2 if self.reject_test else 0
        if selected is self.original and self.reject_restore:
            return -1
        self.active = selected
        return 0


class DisplayTests(unittest.TestCase):
    def test_abi_and_opt_in(self):
        self.assertEqual(ctypes.sizeof(DevMode), 220)
        self.assertEqual(DevMode.width.offset, 172)
        env = dict(SSC_CI_HD_DISPLAY='1', GITHUB_ACTIONS='true', RUNNER_ENVIRONMENT='github-hosted')
        self.assertTrue(allowed(True, env, 'win32'))
        for key in env:
            self.assertFalse(allowed(True, {k:v for k,v in env.items() if k!=key}, 'win32'))
        self.assertFalse(allowed(False, env, 'win32'))
        self.assertFalse(allowed(True, env, 'linux'))

    def test_candidates(self):
        original=mode(1024,768)
        modes=[mode(1920,1080),mode(1280,720,75),mode(1280,720),mode(1280,720),
               mode(1280,720,bpp=16),mode(1280,720,flags=2),mode(1024,768),mode(2560,1440)]
        self.assertEqual([signature(m) for m in candidates(original,modes)],
                         [signature(mode(1280,720)),signature(mode(1280,720,75)),signature(mode(1920,1080))])

    def test_restore_even_after_child_failure(self):
        api=FakeDisplay([mode(1280,720)])
        with self.assertRaisesRegex(RuntimeError, 'child failed'):
            with temporary_mode(api, lambda _:None):
                self.assertEqual(api.current().width,1280)
                raise RuntimeError('child failed')
        self.assertIs(api.current(),api.original)
        self.assertEqual(api.calls,[(signature(mode(1280,720)),True),
                                   (signature(mode(1280,720)),False),(signature(api.original),False)])

    def test_unsupported_or_unneeded_no_mutation(self):
        for api in (FakeDisplay([]),FakeDisplay([mode(1280,720)],reject_test=True),
                    FakeDisplay([mode(1280,720)],single=False),FakeDisplay([],original=mode(1920,1080))):
            with temporary_mode(api,lambda _:None):
                self.assertIs(api.current(),api.original)
            self.assertTrue(all(test for _,test in api.calls))

    def test_restore_failure_is_failure(self):
        api=FakeDisplay([mode(1280,720)],reject_restore=True)
        with self.assertRaisesRegex(RuntimeError,'restoration failed'):
            with temporary_mode(api,lambda _:None):
                pass


if __name__=='__main__':
    unittest.main()
