"""Compare an independently saved C baseline against the C++ application.

python tests/test_cpp_equivalence.py --reference-root PATH_TO_BASELINE
The baseline must contain its ORIGINAL tests/build_simulations.py and C App files.
Only host DLLs are executed. No device access or firmware flashing is performed.
"""
import argparse
import ctypes as C
import hashlib
import heapq
import json
import os
from pathlib import Path
import random
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
P = C.POINTER(C.c_uint8)


class Simulation:
    def __init__(self, path):
        self.lib = C.CDLL(str(path.resolve()))
        signatures = {
            'sim_init': ([C.c_uint32], None),
            'sim_step': ([C.c_uint32], None),
            'sim_receive': ([C.c_uint, P, C.c_uint], None),
            'sim_take_tx': ([C.c_uint, P, C.c_uint], C.c_uint),
            'sim_value': ([C.c_uint], C.c_uint32),
            'sim_set_keys': ([C.c_uint], None),
            'sim_set_joystick': ([C.c_uint16, C.c_uint16], None),
            'sim_fault': ([C.c_uint, C.c_bool], None),
            'sim_screen': ([P], None),
        }
        for name, (args, result) in signatures.items():
            function = getattr(self.lib, name)
            function.argtypes = args
            function.restype = result
        self.buffer = (C.c_uint8 * 1024)()

    def take(self, port):
        count = self.lib.sim_take_tx(port, self.buffer, len(self.buffer))
        return bytes(self.buffer[:count])

    def receive(self, port, data):
        buffer = (C.c_uint8 * len(data)).from_buffer_copy(data)
        self.lib.sim_receive(port, buffer, len(data))

    def screen(self):
        self.lib.sim_screen(self.buffer)
        return bytes(self.buffer)


def check(reference, candidate, label, duration, start=0, stress=False):
    rng = random.Random(0xC0FFEE)
    digest = hashlib.sha256()
    for group in (reference, candidate):
        for sim in group:
            sim.lib.sim_init(start)
    queue = []
    serial = 0
    uart_frames = 0
    screenshots = 0
    for elapsed in range(duration):
        now = (start + elapsed) & 0xFFFFFFFF
        if stress:
            if elapsed in (6000, 12000):
                side = 0 if elapsed == 6000 else 1
                reference[side].lib.sim_init(now)
                candidate[side].lib.sim_init(now)
            if elapsed % 137 == 0:
                x, y = rng.randrange(4096), rng.randrange(4096)
                for group in (reference, candidate):
                    group[0].lib.sim_set_joystick(x, y)
            # All keys, bounce, double clicks and a held key, without changing
            # production input timing. Initial key events still reach the menu.
            phase = elapsed % 800
            keys = (1 << ((elapsed // 800) % 4)) if phase in range(100, 150) or phase in range(230, 280) else 0
            if 3000 <= elapsed < 5400:
                keys = 4
            for group in (reference, candidate):
                group[0].lib.sim_set_keys(keys)
            for at, device, failed in ((3500, 0, True), (3900, 0, False),
                                      (4500, 1, True), (4900, 1, False)):
                if elapsed == at:
                    for group in (reference, candidate):
                        group[0].lib.sim_fault(device, failed)
            if elapsed in (2000, 7000, 12500):
                command = b'F' if elapsed == 7000 else b'T'
                reference[1].receive(1, command)
                candidate[1].receive(1, command)
            if elapsed % 521 == 0:
                noise = bytes(rng.randrange(256) for _ in range(31))
                reference[1].receive(0, noise)
                candidate[1].receive(0, noise)
        outage = stress and 9500 <= elapsed < 11000
        while queue and queue[0][0] <= elapsed:
            _, _, target, data = heapq.heappop(queue)
            if not outage:
                reference[target].receive(0, data)
                candidate[target].receive(0, data)
        for side in (0, 1):
            before, after = reference[side], candidate[side]
            before.lib.sim_step(now)
            after.lib.sim_step(now)
            for index in range(10):
                a, b = before.lib.sim_value(index), after.lib.sim_value(index)
                assert a == b, (label, elapsed, side, 'state', index, a, b)
            for port in (0, 1):
                # Hold TX completion long enough to exercise busy/backpressure
                # and the UART watchdog, symmetrically in both implementations.
                if stress and port == 1 and (2600 <= elapsed < 2750 or 7800 <= elapsed < 7950):
                    continue
                a, b = before.take(port), after.take(port)
                assert a == b, (label, elapsed, side, 'UART', port, a, b)
                if not a:
                    continue
                uart_frames += 1
                digest.update(now.to_bytes(4, 'little') + bytes((side, port)) + a)
                if port == 0 and not outage:
                    serial += 1
                    if stress and serial % 17 == 0:
                        continue
                    if stress and serial % 29 == 0:
                        a = a[:5] + bytes((a[5] ^ 1,)) + a[6:]
                    # Deliberately fragment frames and occasionally duplicate
                    # them. Queue order and inputs are identical for both DLLs.
                    cut = 3 if stress else len(a)
                    heapq.heappush(queue, (elapsed + 4, serial * 3, 1 - side, a[:cut]))
                    if a[cut:]:
                        heapq.heappush(queue, (elapsed + 5, serial * 3 + 1, 1 - side, a[cut:]))
                    if stress and serial % 41 == 0:
                        heapq.heappush(queue, (elapsed + 7, serial * 3 + 2, 1 - side, a))
            if elapsed % 20 == 0:
                a, b = before.screen(), after.screen()
                assert a == b, (label, elapsed, side, 'OLED framebuffer')
                screenshots += 1
                digest.update(a)
    return {'scenario': label, 'milliseconds': duration, 'uart_frames_compared': uart_frames,
            'framebuffers_compared': screenshots, 'state_values_compared': duration * 20,
            'trace_sha256': digest.hexdigest(), 'result': 'identical'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-root', required=True, type=Path)
    parser.add_argument('--skip-build', action='store_true', help='Use existing host DLLs')
    parser.add_argument('--out', type=Path, default=ROOT / 'build/equivalence-results.json')
    args = parser.parse_args()
    baseline = args.reference_root.resolve()
    assert baseline != ROOT, 'Reference must be an independent pre-refactor snapshot'
    for side in ('sender', 'receiver'):
        assert (baseline / side / 'App' / ('app_' + side + '.c')).exists(), 'Expected original C baseline'
    # Do not inherit INVC_BUILD_DIR: loading the same DLL twice would invalidate
    # the comparison. Each tree must produce its own independent binaries.
    if not args.skip_build:
        env = os.environ.copy()
        env.pop('INVC_BUILD_DIR', None)
        for tree in (baseline, ROOT):
            subprocess.run([sys.executable, str(tree / 'tests/build_simulations.py')], env=env, check=True)
    groups = [[Simulation(tree / 'build/tests' / (side + '_sim.dll'))
               for side in ('sender', 'receiver')] for tree in (baseline, ROOT)]
    assert all(a.lib._handle != b.lib._handle for a, b in zip(*groups))
    results = [check(*groups, 'clean_sequence_wrap', 9000),
               check(*groups, 'faults_inputs_loss_restarts_pc_modes', 18000, stress=True),
               check(*groups, 'tick_wrap_and_faults', 14000, start=0xFFFFF000, stress=True)]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(results, indent=2))
    print('C versus C++: identical UART bytes, observable states/counters, and OLED pixels.')


if __name__ == '__main__':
    main()
