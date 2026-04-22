import unittest
import sys
import os

# Adjust path to import from src
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from src.utils import split_tts_sentence, JSONSpeechExtractor

class TestUtils(unittest.TestCase):

    # =========================
    # TESTS split_tts_sentence
    # =========================

    def test_split_tts_sentence_happy_path(self):
        buffer = "Hello world! This is a test."
        # "Hello world!" is 12 chars, match.end() will be 12.
        sentence, remaining = split_tts_sentence(buffer)
        self.assertEqual(sentence, "Hello world!")
        self.assertEqual(remaining, " This is a test.")

    def test_split_tts_sentence_too_short(self):
        buffer = "Hi! How are you?"
        # "Hi!" is 3 chars. 3 < 10, so it should NOT split.
        sentence, remaining = split_tts_sentence(buffer)
        self.assertIsNone(sentence)
        self.assertEqual(remaining, buffer)

    def test_split_tts_sentence_at_threshold(self):
        # "123456789!" -> length 10. match.end() is 10.
        buffer = "123456789! Next part."
        sentence, remaining = split_tts_sentence(buffer)
        self.assertEqual(sentence, "123456789!")
        self.assertEqual(remaining, " Next part.")

    def test_split_tts_sentence_just_below_threshold(self):
        # "12345678!" -> length 9. match.end() is 9.
        buffer = "12345678! Next part."
        sentence, remaining = split_tts_sentence(buffer)
        self.assertIsNone(sentence)
        self.assertEqual(remaining, buffer)

    def test_split_tts_sentence_no_punctuation(self):
        buffer = "This is a long buffer without any punctuation"
        sentence, remaining = split_tts_sentence(buffer)
        self.assertIsNone(sentence)
        self.assertEqual(remaining, buffer)

    def test_split_tts_sentence_multiple_punctuation(self):
        buffer = "Multiple... punctuation!!! After 10 chars."
        # "Multiple..." is 11 chars.
        sentence, remaining = split_tts_sentence(buffer)
        self.assertEqual(sentence, "Multiple...")
        self.assertEqual(remaining, " punctuation!!! After 10 chars.")

    def test_split_tts_sentence_empty(self):
        buffer = ""
        sentence, remaining = split_tts_sentence(buffer)
        self.assertIsNone(sentence)
        self.assertEqual(remaining, "")

    # =========================
    # TESTS JSONSpeechExtractor
    # =========================

    def test_extractor_full_cycle(self):
        extractor = JSONSpeechExtractor()

        # Phase 1: WAIT
        res = extractor.extract_chunk('{"status": "ok", "speech": "')
        self.assertEqual(res, "")
        self.assertEqual(extractor.state, 'EXTRACTING')

        # Phase 2: EXTRACTING
        res = extractor.extract_chunk('Hello ')
        self.assertEqual(res, "Hello")
        self.assertEqual(extractor.buffer, " ")

        res = extractor.extract_chunk('world')
        self.assertEqual(res, " worl") # Extract chunk returns buffer[:-1], current buffer was " world", so " worl"
        self.assertEqual(extractor.buffer, "d")

        # Phase 3: FINISHED
        res = extractor.extract_chunk('!"}')
        self.assertEqual(res, "d!")
        self.assertEqual(extractor.state, 'FINISHED')
        self.assertEqual(extractor.buffer, "}")

    def test_extractor_escaped_quotes(self):
        extractor = JSONSpeechExtractor()
        extractor.extract_chunk('"speech": "')

        # Should not finish on \"
        res = extractor.extract_chunk('He said \\"Hello\\"')
        self.assertNotEqual(extractor.state, 'FINISHED')

        # Should finish on "
        res = extractor.extract_chunk('"')
        self.assertEqual(extractor.state, 'FINISHED')

    def test_extractor_buffer_cleanup_wait(self):
        extractor = JSONSpeechExtractor()
        # Send more than 100 chars of junk
        junk = "a" * 110
        extractor.extract_chunk(junk)
        self.assertLessEqual(len(extractor.buffer), 100)
        self.assertEqual(extractor.buffer, "a" * 50)

if __name__ == '__main__':
    unittest.main()
