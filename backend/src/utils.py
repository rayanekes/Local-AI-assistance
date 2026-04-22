import re

class JSONSpeechExtractor:
    def __init__(self):
        self.state = 'WAIT'
        self.buffer = ""

    def extract_chunk(self, token):
        self.buffer += token
        if self.state == 'WAIT':
            match = re.search(r'"speech"\s*:\s*"', self.buffer)
            if match:
                self.state = 'EXTRACTING'
                self.buffer = self.buffer[match.end():]
            elif len(self.buffer) > 100:
                self.buffer = self.buffer[-50:]
            return ""

        if self.state == 'EXTRACTING':
            match = re.search(r'(?<!\\)"', self.buffer)
            if match:
                self.state = 'FINISHED'
                speech = self.buffer[:match.start()]
                self.buffer = self.buffer[match.end():]
                return speech
            if len(self.buffer) > 1:
                speech = self.buffer[:-1]
                self.buffer = self.buffer[-1:]
                return speech
        return ""

def split_tts_sentence(buffer):
    match = re.search(r'([.?!]+)', buffer)
    if match and match.end() >= 10:
        return buffer[:match.end()], buffer[match.end():]
    return None, buffer
