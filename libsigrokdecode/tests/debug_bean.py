import sys

class MockSrd:
    OUTPUT_ANN = 1
    OUTPUT_PYTHON = 2
    OUTPUT_BINARY = 3
    OUTPUT_META = 4
    class Decoder:
        def __init__(self): pass
        def register(self, t): return t
        def put(self, *args): pass
        def wait(self, *args): return None

sys.modules['sigrokdecode'] = MockSrd

sys.path.insert(0, '../decoders')
import bean.pd as pd

class MockOutput:
    def __init__(self):
        self.anns = []
    def put(self, ss, es, out_ann, data):
        self.anns.append((ss, es, data))

d = pd.Decoder()
d.options = {'bit_annotations': 'yes', 'pulse_len': 'yes', 'command': 'yes', 'all byte': 'yes'}
d.out_ann = 1
d.put = MockOutput().put

import struct
with open('testdata/bean_c/default/input.bin', 'rb') as f:
    data = f.read()

class Sampler:
    def __init__(self, data):
        self.data = data
        self.bit_idx = 0
        self.samplenum = 0
        self.last_state = -1
    def get_next_edge(self):
        while self.bit_idx < len(self.data) * 8:
            byte_idx = self.bit_idx // 8
            bit_offset = self.bit_idx % 8
            state = (self.data[byte_idx] >> bit_offset) & 1
            if self.last_state == -1:
                self.last_state = state
            elif state != self.last_state:
                self.last_state = state
                return state
            self.samplenum += 1
            self.bit_idx += 1
        return None

s = Sampler(data)
d.samplenum = 0
d.pin = 0

def wait_mock(cond=None):
    state = s.get_next_edge()
    if state is None:
        raise EOFError()
    d.samplenum = s.samplenum
    d.pin = state
    return (state,)

d.wait = wait_mock

try:
    d.start()
    d.decode()
except EOFError:
    pass
except Exception as e:
    import traceback
    traceback.print_exc()

print("Anns:", len(d.put.__self__.anns))
for a in d.put.__self__.anns:
    print(a)
