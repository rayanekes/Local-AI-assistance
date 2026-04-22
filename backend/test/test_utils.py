import pytest
import os
import sys
import unittest.mock as mock

# Add backend directory to sys.path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

# Create mock objects
mock_sys_exit = mock.MagicMock()
mock_torch = mock.MagicMock()
mock_faster_whisper = mock.MagicMock()
mock_silero_vad = mock.MagicMock()
mock_llama_cpp = mock.MagicMock()
mock_shutil_which = mock.MagicMock(return_value="/mock/path/to/piper")

# Define the dictionary of modules to mock
mock_modules = {
    'torch': mock_torch,
    'faster_whisper': mock_faster_whisper,
    'silero_vad': mock_silero_vad,
    'llama_cpp': mock_llama_cpp,
}

# Use patch.dict for sys.modules and patch for sys.exit and shutil.which
with mock.patch.dict(sys.modules, mock_modules), \
     mock.patch('sys.exit', mock_sys_exit), \
     mock.patch('shutil.which', mock_shutil_which):

    import src.server as server

class TestJSONSpeechExtractor:
    def setup_method(self):
        self.extractor = server.JSONSpeechExtractor()

    def test_wait_state_no_match(self):
        token = '{"other": "value", '
        result = self.extractor.extract_chunk(token)
        assert result == ""
        assert self.extractor.state == 'WAIT'

    def test_wait_state_buffer_truncation(self):
        token = "a" * 150
        result = self.extractor.extract_chunk(token)
        assert result == ""
        assert len(self.extractor.buffer) == 50

    def test_wait_to_extracting_transition(self):
        # Sending chunks that eventually form the pattern
        assert self.extractor.extract_chunk('{"s') == ""
        assert self.extractor.extract_chunk('peech":') == ""
        assert self.extractor.extract_chunk(' "') == ""

        assert self.extractor.state == 'EXTRACTING'
        # Buffer should be cleared of the matched pattern
        assert self.extractor.buffer == ""

    def test_extracting_streaming(self):
        self.extractor.state = 'EXTRACTING'

        # When sending "hello ", it should return "hello" and keep " " in buffer
        result1 = self.extractor.extract_chunk('hello ')
        assert result1 == 'hello'
        assert self.extractor.buffer == ' '

        # When sending "world", it should return " worl" and keep "d" in buffer
        result2 = self.extractor.extract_chunk('world')
        assert result2 == ' worl'
        assert self.extractor.buffer == 'd'

    def test_extracting_to_finished(self):
        self.extractor.state = 'EXTRACTING'
        self.extractor.buffer = 'hello '

        # Sending the closing quote
        result = self.extractor.extract_chunk('world"')
        assert result == 'hello world'
        assert self.extractor.state == 'FINISHED'
        assert self.extractor.buffer == ''

    def test_extracting_escaped_quote(self):
        self.extractor.state = 'EXTRACTING'

        # Sending escaped quote, should stream characters up to the last one
        result = self.extractor.extract_chunk('hello \\"world')
        assert self.extractor.state == 'EXTRACTING'
        assert result == 'hello \\"worl'
        assert self.extractor.buffer == 'd'

    def test_full_json_extraction(self):
        tokens = [
            '{', '\n  ', '"speech"', ': "', 'Bonjour', ' le ', 'monde!', '\\" ', 'C\'est ', 'super.', '",\n',
            '  "emotion": "joie"\n}'
        ]

        extracted = []
        for token in tokens:
            extracted.append(self.extractor.extract_chunk(token))

        final_speech = "".join(extracted)
        assert final_speech == 'Bonjour le monde!\\" C\'est super.'
        assert self.extractor.state == 'FINISHED'
