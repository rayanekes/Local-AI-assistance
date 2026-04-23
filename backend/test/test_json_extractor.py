import unittest
import sys
import os
import re

# Add the src directory to the path so we can import utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))
from utils import JSONSpeechExtractor

class TestJSONSpeechExtractor(unittest.TestCase):
    def setUp(self):
        self.extractor = JSONSpeechExtractor()

    def test_basic_extraction(self):
        tokens = ['{', '"speech"', ':', '"', 'Hello world', '"', '}']
        extracted = ""
        for t in tokens:
            extracted += self.extractor.extract_chunk(t)
        self.assertEqual(extracted, "Hello world")
        self.assertEqual(self.extractor.state, 'FINISHED')

    def test_tokenized_extraction(self):
        # Extremely fragmented tokens
        full_json = '{"speech": "This is a test message."}'
        extracted = ""
        for char in full_json:
            extracted += self.extractor.extract_chunk(char)
        self.assertEqual(extracted, "This is a test message.")
        self.assertEqual(self.extractor.state, 'FINISHED')

    def test_escaped_quotes(self):
        # The extractor returns the content between the first "speech":" and the next unescaped "
        tokens = ['{"speech": "He said \\"hello\\" to me"']
        extracted = ""
        for t in tokens:
            extracted += self.extractor.extract_chunk(t)

        # Add the closing quote and closing brace
        extracted += self.extractor.extract_chunk('}')

        self.assertEqual(extracted, 'He said \\"hello\\" to me')
        self.assertEqual(self.extractor.state, 'FINISHED')

    def test_buffer_overflow_wait(self):
        # If we get a lot of junk before "speech"
        junk = "A" * 150
        self.extractor.extract_chunk(junk)
        self.assertEqual(self.extractor.state, 'WAIT')
        self.assertLessEqual(len(self.extractor.buffer), 100)

        # Now send the real thing
        tokens = ['{"speech": "Finally"']
        extracted = ""
        for t in tokens:
            extracted += self.extractor.extract_chunk(t)
        extracted += self.extractor.extract_chunk('}')
        self.assertEqual(extracted, "Finally")

    def test_incremental_speech(self):
        # Test if it returns chunks of speech as they arrive
        self.extractor.extract_chunk('{"speech": "Hello ')
        chunk1 = self.extractor.extract_chunk('world')
        # "Hello " was in buffer after first chunk (actually "ello " because "speech": " matches and sets buffer to everything after)
        # Actually:
        # 1. extract_chunk('{"speech": "Hello ')
        #    buffer becomes '{"speech": "Hello '
        #    match found, state -> EXTRACTING
        #    buffer becomes 'Hello '
        #    Since state is EXTRACTING now (in the SAME call? No, it returns "" after setting state)

        # Let's trace extract_chunk('{"speech": "Hello ')
        # self.buffer = '{"speech": "Hello '
        # match found for '"speech"\s*:\s*"'
        # state = 'EXTRACTING'
        # self.buffer = 'Hello '
        # return ""

        # Then extract_chunk('world')
        # self.buffer = 'Hello world'
        # match NOT found for unescaped "
        # len(self.buffer) > 1:
        # speech = 'Hello worl'
        # self.buffer = 'd'
        # return 'Hello worl'

        # Then extract_chunk('!"}')
        # self.buffer = 'd!"}'
        # match found for " at index 2
        # state = 'FINISHED'
        # speech = 'd!'
        # self.buffer = '}'
        # return 'd!'

        self.extractor = JSONSpeechExtractor()
        res1 = self.extractor.extract_chunk('{"speech": "Hello ')
        self.assertEqual(res1, "")

        res2 = self.extractor.extract_chunk('world')
        self.assertEqual(res2, "Hello worl")

        res3 = self.extractor.extract_chunk('!"}')
        self.assertEqual(res3, "d!")
        self.assertEqual(self.extractor.state, 'FINISHED')

if __name__ == '__main__':
    unittest.main()
