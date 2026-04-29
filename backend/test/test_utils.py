import pytest
from backend.src.utils import JSONSpeechExtractor, split_tts_sentence

def test_split_tts_sentence():
    # Case with punctuation and enough length
    sentence, remainder = split_tts_sentence("This is a long sentence. More text")
    assert sentence == "This is a long sentence."
    assert remainder == " More text"

    # Case with punctuation but too short
    sentence, remainder = split_tts_sentence("Hi! ok")
    assert sentence is None
    assert remainder == "Hi! ok"

    # Case with no punctuation
    sentence, remainder = split_tts_sentence("No punctuation here")
    assert sentence is None
    assert remainder == "No punctuation here"

    # Multiple punctuations
    sentence, remainder = split_tts_sentence("Wait for it... Done.")
    assert sentence == "Wait for it..."
    assert remainder == " Done."

def test_json_speech_extractor_basic():
    extractor = JSONSpeechExtractor()

    # Send tokens one by one
    tokens = ['{', '"speech"', ':', '"', 'Hello', ' world', '"', ',', '"emotion"', ':', '"joy"', '}']
    extracted = ""
    for token in tokens:
        extracted += extractor.extract_chunk(token)

    assert extracted == "Hello world"
    assert extractor.state == 'FINISHED'

def test_json_speech_extractor_escaped_quotes():
    extractor = JSONSpeechExtractor()

    # JSON with escaped quotes: {"speech": "He said \"Hello\""}
    tokens = ['{', '"speech"', ':', '"', 'He', ' said', ' \\"', 'Hello', '\\"', '"', '}']
    extracted = ""
    for token in tokens:
        extracted += extractor.extract_chunk(token)

    assert extracted == 'He said \\"Hello\\"'
    # The actual implementation doesn't unescape, but correctly identifies the end of the string
    assert extractor.state == 'FINISHED'

def test_json_speech_extractor_buffer_overflow_protection():
    extractor = JSONSpeechExtractor()

    # Send a lot of junk before "speech"
    junk = "a" * 150
    for char in junk:
        extractor.extract_chunk(char)

    assert extractor.state == 'WAIT'
    assert len(extractor.buffer) <= 100

    # Now send the actual trigger
    tokens = ['"', 'speech', '"', ':', '"', 'Finally', '"']
    extracted = ""
    for token in tokens:
        extracted += extractor.extract_chunk(token)

    assert extracted == "Finally"
    assert extractor.state == 'FINISHED'
